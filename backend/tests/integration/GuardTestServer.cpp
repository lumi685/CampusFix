// Isolated test executable. These routes are never compiled into campusfix_backend.
#include "middleware/AuthGuard.h"
#include "common/AccessResponse.h"
#include "controllers/AuthController.h"
#include "controllers/HealthController.h"
#include "infrastructure/AuthCrypto.h"
#include "infrastructure/DatabaseMigration.h"
#include "infrastructure/DemoSeed.h"
#include "services/AuthorizationService.h"
#include "services/DemoAccountActivation.h"
#include <fstream>
#include <iostream>

using namespace campusfix;
int main(int argc,char **argv) {
    if(argc!=6) return 2;
    try {
        initializeCrypto();const std::filesystem::path path(argv[1]);
        applyMigrations(path,argv[3]);applyDemoSeed(path,argv[4]);
        Json::Value fixture(Json::objectValue);
        const auto password=newToken();
        setDemoPassword(path,"demo_reporter01",password);
        enableDemoAccount(path,"demo_reporter01");
        fixture["password"]=password;
        {
            SqliteConnection db(path);
            db.execute("UPDATE users SET status='ACTIVE' WHERE username IN ('demo_reporter01','demo_worker01','demo_dispatcher','demo_sysadmin')");
            AuthRepository repo(db);
            for(const auto &entry:std::vector<std::pair<std::string,std::string>>{
                {"reporter","demo_reporter01"},{"worker","demo_worker01"},{"dispatcher","demo_dispatcher"},
                {"admin","demo_sysadmin"},{"expired","demo_reporter01"},{"revoked","demo_reporter01"},{"disabled","demo_reporter02"}}) {
                const auto user=repo.userByName(entry.second);const auto token=newToken();
                repo.createSession(newOpaqueId(),user.id,tokenDigest(token),
                    entry.first=="expired"?"2000-01-01T00:00:00.000Z":"2099-01-01T00:00:00.000Z");
                if(entry.first=="revoked") repo.revokeSession(tokenDigest(token));
                fixture[entry.first]=token;
                fixture[entry.first+"_id"]=Json::Int64(user.id);
            }
            fixture["order_a"]=Json::Int64(db.scalarInt("SELECT id FROM work_orders WHERE order_no='WX202001010001'"));
            fixture["order_b"]=Json::Int64(db.scalarInt("SELECT id FROM work_orders WHERE order_no='WX202001010002'"));
        }
        registerHealthController();registerAuthController(path);
        drogon::app().registerHandler("/test/identity",authenticated(path,
            [](const drogon::HttpRequestPtr &req,HttpCallback &&cb){
                const auto identity=requestIdentity(req);Json::Value data(Json::objectValue);
                data["id"]=Json::Int64(identity->id);data["role"]=identity->role;
                cb(successResponse(data,identity->requestId));
            }),{drogon::Get});
        drogon::app().registerHandler("/test/unconfigured",authenticated({},
            [](const drogon::HttpRequestPtr &,HttpCallback &&){throw std::runtime_error("Guard bypassed");}),{drogon::Get});
        drogon::app().registerHandler("/test/order-access",authenticated(path,
            [path](const drogon::HttpRequestPtr &req,HttpCallback &&cb){
                const auto identity=requestIdentity(req);
                // Test-only endpoints use query parameters for resource/action, never identity.
                try {
                    const auto id=std::stoll(req->getParameter("id"));
                    const auto action=req->getParameter("action")=="review"?Action::Review:Action::Read;
                    SqliteConnection db(path);AuthorizationService auth(db);
                    const auto decision=auth.order(identity->id,action,id);
                    if(!decision.allowed()) {cb(accessErrorResponse(decision,identity->requestId));return;}
                    Json::Value data(Json::objectValue);data["allowed"]=true;
                    cb(successResponse(data,identity->requestId));
                }catch(const std::exception &) {cb(errorResponse(drogon::k500InternalServerError,"INTERNAL_ERROR","Test failed",identity->requestId));}
            }),{drogon::Get});
        std::ofstream output(argv[2],std::ios::binary);output<<fixture;output.close();
        if(!output) throw std::runtime_error("Cannot write isolated fixture");
        drogon::app().setLogLevel(trantor::Logger::kWarn);
        drogon::app().addListener("127.0.0.1",static_cast<std::uint16_t>(std::stoi(argv[5]))).run();
        return 0;
    }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
