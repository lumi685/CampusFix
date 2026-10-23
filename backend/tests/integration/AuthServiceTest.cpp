#include "infrastructure/DatabaseMigration.h"
#include "infrastructure/DemoSeed.h"
#include "infrastructure/AuthCrypto.h"
#include "infrastructure/SqliteConnection.h"
#include "services/DemoAccountActivation.h"
#include "services/AuthService.h"
#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
void require(bool ok,const char *message) { if(!ok) throw std::runtime_error(message); }
template<class Fn> void rejects(Fn fn) { bool failed=false; try {fn();}catch(const std::invalid_argument &){failed=true;} require(failed,"Expected authentication rejection"); }
}
int main() {
    const auto root=std::filesystem::temp_directory_path()/
      ("campusfix-auth-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        std::filesystem::create_directories(root);
        const auto dbPath=root/"auth.sqlite3";
        campusfix::initializeCrypto();
        campusfix::applyMigrations(dbPath,CAMPUSFIX_MIGRATIONS_DIR);
        campusfix::applyDemoSeed(dbPath,CAMPUSFIX_SEED_FILE);
        const auto secret=campusfix::newToken();
        campusfix::setDemoPassword(dbPath,"demo_reporter01",secret);
        campusfix::enableDemoAccount(dbPath,"demo_reporter01");
        campusfix::AuthService auth(dbPath);
        rejects([&]{auth.login("demo_reporter01","bad");});
        rejects([&]{auth.login("missing_user",secret);});
        rejects([&]{auth.login("demo_reporter02",secret);});
        auto first=auth.login("demo_reporter01",secret);
        auto second=auth.login("demo_reporter01",secret);
        require(!first.token.empty() && first.token!=second.token,"Tokens must differ");
        require(auth.me(first.token).user.username=="demo_reporter01","Valid session not found");
        rejects([&]{auth.me("cf_live_invalid_token");});
        {campusfix::SqliteConnection db(dbPath);
         require(db.scalarInt("SELECT COUNT(*) FROM auth_sessions")==2,"Session count differs");
         auto q=db.prepare("SELECT COUNT(*) FROM auth_sessions WHERE token_hash=?1");
         q.bindText(1,first.token); require(q.step()&&q.columnInt(0)==0,"Raw token persisted");}
        require(auth.logout(first.token),"Logout failed");
        rejects([&]{auth.me(first.token);});
        require(auth.me(second.token).user.id>0,"Other session revoked");
        campusfix::AuthService restarted(dbPath);
        require(restarted.me(second.token).user.id>0,"Session lost across service instance");
        {campusfix::SqliteConnection db(dbPath);auto q=db.prepare("UPDATE auth_sessions SET expires_at='2000-01-01T00:00:00.000Z' WHERE token_hash=?1");q.bindText(1,campusfix::tokenDigest(second.token));q.step();}
        rejects([&]{restarted.me(second.token);});
        {campusfix::SqliteConnection db(dbPath);
         db.execute("CREATE TRIGGER fail_session BEFORE INSERT ON auth_sessions BEGIN SELECT RAISE(ABORT,'test'); END");
         auto count=db.scalarInt("SELECT COUNT(*) FROM auth_sessions");
         auto before=db.prepare("SELECT last_login_at FROM users WHERE username='demo_reporter01'");before.step();const auto last=before.columnText(0);
         bool failed=false;try{auth.login("demo_reporter01",secret);}catch(const std::exception &){failed=true;}
         require(failed && db.scalarInt("SELECT COUNT(*) FROM auth_sessions")==count,"Failed session left a row");
         auto after=db.prepare("SELECT last_login_at FROM users WHERE username='demo_reporter01'");after.step();require(after.columnText(0)==last,"Failed login updated timestamp");}
        std::filesystem::remove_all(root);
        std::cout<<"PASS authentication, persistence, revocation, expiry and rollback\n";return 0;
    } catch(const std::exception &error) {std::cerr<<error.what()<<'\n';return 1;}
}
