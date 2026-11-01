#include "WorkerTaskTestData.h"
#include "services/TransferQueryService.h"
#include <iostream>
#include <limits>
using namespace campusfix;
namespace {
int checks=0;
struct ConcurrentRead {std::filesystem::path path;AssignmentResult order;std::int64_t user=0;bool inserted=false,failed=false;};
int insertDuringRows(unsigned,void *context,void *statement,void *) {
    auto &c=*static_cast<ConcurrentRead *>(context);const auto *sql=sqlite3_sql(static_cast<sqlite3_stmt *>(statement));
    if(!c.inserted&&sql&&std::string(sql).find("SELECT t.id,t.order_id")!=std::string::npos){c.inserted=true;try{SqliteConnection writer(c.path);test::pendingTransfer(writer,c.order,c.user);}catch(...){c.failed=true;}}
    return 0;
}
void check(bool value,const char *message){if(!value)throw std::runtime_error(message);++checks;}
TransferPage read(const std::filesystem::path &path,const test::ReadAccount &account,TransferQuery q={"mine"},int error=0) {
    SqliteConnection before(path);const auto fingerprint=test::dispatchFingerprint(before);TransferPage result;bool rejected=false;
    {SqliteConnection db(path);try{result=TransferQueryService(db).list(account.id,account.token,q);}catch(const WorkOrderError &e){rejected=e.status==error;}check(db.scalarInt("PRAGMA query_only")==1,"dedicated query-only connection");}
    check(rejected==(error!=0),"expected status");check(fingerprint==test::dispatchFingerprint(before),"query success/failure leaves all16 tables unchanged");return result;
}
}
int main(){initializeCrypto();const auto root=std::filesystem::temp_directory_path()/("campusfix-transfer-query-"+newOpaqueId());std::filesystem::create_directories(root);int code=0;
    try {
        const auto path=root/"test.sqlite3";const auto f=test::taskFixture(path,CAMPUSFIX_MIGRATIONS_DIR,CAMPUSFIX_SEED_FILE);SqliteConnection db(path);
        const auto one=test::taskOrder(db,f),two=test::taskOrder(db,f,2);test::pendingTransfer(db,one,f.accounts.at("demo_worker01").id);test::pendingTransfer(db,two,f.accounts.at("demo_worker02").id);
        const auto &worker=f.accounts.at("demo_worker01"),&admin=f.accounts.at("demo_sysadmin"),&dispatcher=f.accounts.at("demo_dispatcher");
        auto mine=read(path,worker);check(mine.total==1&&mine.items[0].orderId==one.orderId,"worker sees only own request");
        read(path,f.accounts.at("demo_reporter01"),{"mine"},403);read(path,worker,{"managed"},403);read(path,admin,{"managed"},403);
        test::grant(db,"demo_sysadmin","BUSINESS_READ",1);read(path,admin,{"managed"},403);test::grant(db,"demo_sysadmin","DISPATCH",1);
        auto managed=read(path,admin,{"managed"});check(managed.total==2,"DISPATCH works without internal note permission");check(read(path,dispatcher,{"managed"}).total==2,"dispatcher scope");
        test::grant(db,"demo_sysadmin","DISPATCH",3);check(read(path,admin,{"managed"}).total==2,"overlapping grants do not duplicate");
        TransferQuery filter{"managed"};filter.orderId=999999;check(read(path,admin,filter).total==0,"unknown filter empty");
        db.execute("UPDATE work_orders SET location_id=13 WHERE id="+std::to_string(two.orderId));filter.orderId=two.orderId;check(read(path,admin,filter).total==0,"foreign filter also empty");
        check(read(path,admin,{"managed"}).total==1,"cross area excluded");
        test::taskAction(db,worker,one.orderId);mine=read(path,worker);check(mine.items[0].orderVersion==5,"returns current version after real accept");
        check(db.scalarInt("SELECT order_version FROM transfer_requests WHERE assignment_id="+std::to_string(one.assignmentId))==4,"stored application snapshot unchanged");
        db.execute("UPDATE transfer_requests SET status='REJECTED',review_reason='Fictional rejection',resolved_at='2026-11-01T00:00:00.000Z' WHERE assignment_id="+std::to_string(one.assignmentId));
        check(read(path,admin,{"managed"}).total==0,"resolved excluded");mine=read(path,worker);check(mine.items[0].status=="REJECTED"&&mine.items[0].reviewReason=="Fictional rejection","mine history public decision");
        db.execute("UPDATE transfer_requests SET status='PENDING' WHERE assignment_id="+std::to_string(one.assignmentId));db.execute("UPDATE assignments SET ended_at='2026-11-01T00:00:00.000Z',end_reason='TRANSFERRED' WHERE id="+std::to_string(one.assignmentId));
        check(read(path,admin,{"managed"}).total==0,"ended assignments excluded");
        db.execute("UPDATE assignments SET ended_at=NULL,end_reason=NULL WHERE id="+std::to_string(one.assignmentId));db.execute("UPDATE work_orders SET status='PENDING_VERIFY' WHERE id="+std::to_string(one.orderId));
        check(read(path,admin,{"managed"}).total==0,"wrong order state excluded");
        test::revoke(db,"demo_sysadmin","DISPATCH");read(path,admin,{"managed"},403);db.execute("UPDATE users SET status='DISABLED' WHERE username='demo_worker01'");read(path,worker,{"mine"},401);
        for(const auto &q:std::vector<TransferQuery>{{"wrong"},{"managed",0,20,{}},{"managed",1,101,{}},{"managed",std::numeric_limits<std::int64_t>::max(),100,{}}})read(path,dispatcher,q,422);
        {SqliteConnection only(path);only.execute("PRAGMA query_only=ON");bool blocked=false;try{only.execute("UPDATE users SET display_name='Should not write'");}catch(const SqliteError &e){blocked=e.primaryCode()==SQLITE_READONLY;}check(blocked,"query_only actually blocks writes");}
        db.execute("UPDATE users SET status='ACTIVE' WHERE username='demo_worker01'");db.execute("PRAGMA journal_mode=WAL");
        const auto concurrentOrder=test::taskOrder(db,f);const auto old=read(path,worker).total;
        ConcurrentRead context{path,concurrentOrder,worker.id};{SqliteConnection reader(path);sqlite3_trace_v2(reader.nativeHandle(),SQLITE_TRACE_STMT,insertDuringRows,&context);const auto snapshot=TransferQueryService(reader).list(worker.id,worker.token,{"mine"});check(context.inserted&&!context.failed,"separate writer commits between count and items");check(snapshot.total==old&&static_cast<std::int64_t>(snapshot.items.size())==old,"one consistent read snapshot across count/items");}
        check(read(path,worker).total==old+1,"next snapshot sees separately committed new row");
        test::integrity(db);std::cout<<"PASS transfer query assertions="<<checks<<" integrity_check=ok; foreign_key_check empty\n";
    }catch(const std::exception &e){std::cerr<<"FAIL transfer query "<<e.what()<<'\n';code=1;}
    std::error_code ec;std::filesystem::remove_all(root,ec);return ec?1:code;
}
