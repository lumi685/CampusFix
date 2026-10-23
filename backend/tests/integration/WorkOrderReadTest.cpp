#include "WorkOrderReadTestData.h"
#include "services/WorkOrderService.h"
#include "services/AuthorizationService.h"
#include <algorithm>
#include <iostream>
#include <limits>
#include <set>

using namespace campusfix;
namespace {
int checks=0;
void check(bool ok,const char *name) {if(!ok) throw std::runtime_error(name);++checks;std::cout<<"PASS "<<name<<'\n';}
template<class F> void rejects(F fn,int status,const std::string &code) {
    try {fn();}catch(const WorkOrderError &e) {check(e.status==status&&e.code==code,"expected read rejection");return;}
    throw std::runtime_error("Expected read rejection");
}
std::set<std::int64_t> ids(const OrderPage &page) {std::set<std::int64_t> r;for(const auto &s:page.items) r.insert(s.id);return r;}
}
int main() {
    initializeCrypto();const auto root=std::filesystem::temp_directory_path()/("campusfix-read-"+newOpaqueId());
    try {
        std::filesystem::create_directories(root);const auto path=root/"read.sqlite3";
        const auto fixture=test::readFixture(path,CAMPUSFIX_MIGRATIONS_DIR,CAMPUSFIX_SEED_FILE);
        {
        SqliteConnection db(path);WorkOrderService service(db);const auto baseline=test::readFingerprint(db);
        auto page=[&](const char *name,OrderView view) {
            const auto &a=fixture.accounts.at(name);OrderQuery q;q.view=view;return service.list(a.id,a.token,q);
        };
        auto detail=[&](const char *name,std::int64_t id) {const auto &a=fixture.accounts.at(name);return service.detail(a.id,a.token,id);};
        const auto reporter=page("demo_reporter01",OrderView::Reported);
        check(ids(reporter)==std::set<std::int64_t>{1,fixture.current,fixture.draft}&&reporter.total==3,"reporter A exact SQL scope and total");
        check(ids(page("demo_reporter02",OrderView::Reported))==std::set<std::int64_t>{2,fixture.history,fixture.foreign},"reporter B excludes A");
        check(ids(page("demo_worker01",OrderView::Reported))==std::set<std::int64_t>{4},"worker reported does not include assigned tasks");
        check(ids(page("demo_worker01",OrderView::Assigned))==std::set<std::int64_t>{fixture.current,fixture.history},"worker assigned includes own current and controlled history only");
        check(ids(page("demo_worker02",OrderView::Assigned))==std::set<std::int64_t>{fixture.history,fixture.foreign},"other worker scope");
        check(ids(page("demo_dispatcher",OrderView::Managed))==std::set<std::int64_t>{1,3,fixture.current,fixture.history,fixture.draft},"dispatcher campus scope includes descendants, excludes B");
        check(ids(page("demo_sysadmin",OrderView::Managed))==std::set<std::int64_t>{2,4,fixture.foreign},"sysadmin uses BUSINESS_READ own scope");
        rejects([&]{page("test_no_grants",OrderView::Managed);},403,"ACTION_FORBIDDEN");
        for(const auto &entry:std::vector<std::pair<const char *,OrderView>>{{"demo_reporter01",OrderView::Assigned},{"demo_reporter01",OrderView::Managed},{"demo_worker01",OrderView::Managed},{"demo_dispatcher",OrderView::Reported},{"demo_sysadmin",OrderView::Assigned}})
            rejects([&]{page(entry.first,entry.second);},403,"ACTION_FORBIDDEN");
        const auto &r=fixture.accounts.at("demo_reporter01");OrderQuery q;
        check(reporter.page==1&&reporter.pageSize==20,"default pagination");
        q.pageSize=1;std::vector<std::int64_t> paged;
        for(q.page=1;q.page<=3;++q.page) {const auto p=service.list(r.id,r.token,q);check(p.items.size()==1&&p.total==3,"page size 1 maintains authorized total");paged.push_back(p.items.front().id);}
        check(paged==std::vector<std::int64_t>{fixture.draft,fixture.current,1},"same-time pagination is stable descending id without overlap");
        q.page=4;check(service.list(r.id,r.token,q).items.empty(),"past last page is empty");
        q.page=1;q.pageSize=100;check(service.list(r.id,r.token,q).items.size()==3,"page size 100 boundary");
        q.status="DRAFT";check(ids(service.list(r.id,r.token,q))==std::set<std::int64_t>{fixture.draft},"status filter");
        q.categoryId=2;check(service.list(r.id,r.token,q).total==1,"combined filters");
        q.categoryId=1;check(service.list(r.id,r.token,q).total==0&&service.list(r.id,r.token,q).items.empty(),"valid empty filter result");
        q.status.reset();check(service.list(r.id,r.token,q).total==2,"category filter");
        for(int mode=0;mode<7;++mode) {
            OrderQuery bad;
            switch(mode) {case 0:bad.page=0;break;case 1:bad.pageSize=0;break;case 2:bad.pageSize=101;break;
            case 3:bad.status="PENDING_REVIEW' OR 1=1 --";break;case 4:bad.categoryId=-1;break;
            case 5:bad.page=std::numeric_limits<std::int64_t>::max();break;case 6:bad.view=static_cast<OrderView>(99);break;}
            rejects([&]{service.list(r.id,r.token,bad);},422,"VALIDATION_ERROR");
        }
        const auto own=detail("demo_reporter01",fixture.current);
        check(own.contact&&own.currentAssignment&&own.currentAssignment->workerId==1,"reporter detail authorized contact and minimal current assignment");
        check(detail("demo_worker01",4).contact.has_value(),"worker own report detail per D-017");
        check(detail("demo_worker01",fixture.current).contact.has_value(),"current worker gets necessary contact");
        const auto history=detail("demo_worker01",fixture.history);
        check(!history.contact&&!history.currentAssignment,"historical worker cannot get contacts, description or new worker identity");
        rejects([&]{detail("demo_reporter01",2);},404,"RESOURCE_NOT_FOUND");
        rejects([&]{detail("demo_worker01",fixture.foreign);},404,"RESOURCE_NOT_FOUND");
        rejects([&]{detail("demo_dispatcher",2);},404,"RESOURCE_NOT_FOUND");
        rejects([&]{detail("test_no_grants",fixture.current);},404,"RESOURCE_NOT_FOUND");
        rejects([&]{detail("demo_reporter01",999999);},404,"RESOURCE_NOT_FOUND");
        rejects([&]{detail("demo_reporter01",0);},422,"VALIDATION_ERROR");
        check(detail("demo_dispatcher",fixture.current).contact.has_value(),"dispatcher in-scope detail");
        test::readGrantMode(db,"admin-building");
        check(ids(page("demo_sysadmin",OrderView::Managed))==std::set<std::int64_t>{1,fixture.current,fixture.history,fixture.draft},"building grant covers its room descendants only");
        db.execute("INSERT INTO management_grants(user_id,capability,scope_location_id,granted_by) SELECT id,'BUSINESS_READ',11,id FROM users WHERE username='demo_sysadmin'");
        check(page("demo_sysadmin",OrderView::Managed).total==4,"overlapping parent/child grants do not duplicate rows or totals");
        for(const auto &item:page("demo_sysadmin",OrderView::Managed).items)
            check(AuthorizationService(db).order(fixture.accounts.at("demo_sysadmin").id,Action::Read,item.id).allowed(),"SQL management scope agrees with existing detail Policy");
        check(!AuthorizationService(db).fields(fixture.accounts.at("demo_sysadmin").id,fixture.current).internal,"INTERNAL_NOTE_READ scope B does not combine with BUSINESS_READ scope A");
        test::readGrantMode(db,"admin-room");
        check(page("demo_sysadmin",OrderView::Managed).total==4,"room grant does not expand upward to sibling building");
        test::readGrantMode(db,"admin-global");check(page("demo_sysadmin",OrderView::Managed).total==8,"explicit global BUSINESS_READ");
        test::readGrantMode(db,"admin-read-only");
        check(page("demo_sysadmin",OrderView::Managed).total==3&&!AuthorizationService(db).fields(fixture.accounts.at("demo_sysadmin").id,2).internal,"BUSINESS_READ without INTERNAL_NOTE_READ grants no internal fields");
        test::readGrantMode(db,"revoke-admin");rejects([&]{page("demo_sysadmin",OrderView::Managed);},403,"ACTION_FORBIDDEN");
        rejects([&]{detail("demo_sysadmin",2);},404,"RESOURCE_NOT_FOUND");
        test::readGrantMode(db,"admin-area-only");rejects([&]{page("demo_sysadmin",OrderView::Managed);},403,"ACTION_FORBIDDEN");
        test::readGrantMode(db,"revoke-dispatcher");rejects([&]{page("demo_dispatcher",OrderView::Managed);},403,"ACTION_FORBIDDEN");
        const auto changes=sqlite3_total_changes64(db.nativeHandle());
        for(int n=0;n<3;++n) {page("demo_reporter01",OrderView::Reported);detail("demo_reporter01",fixture.current);}
        check(sqlite3_total_changes64(db.nativeHandle())==changes&&test::readFingerprint(db)==baseline,"reads do not change order state/version or append events");
        test::readGrantMode(db,"disable-reporter");rejects([&]{page("demo_reporter01",OrderView::Reported);},401,"AUTH_INVALID");
        check(db.scalarInt("SELECT count(*) FROM pragma_foreign_key_check")==0,"read fixture foreign keys valid");
        }
        std::filesystem::remove_all(root);std::cout<<"PASS "<<checks<<" read SQLite/Service checks (not HTTP)\n";return 0;
    }catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';std::filesystem::remove_all(root);return 1;}
}
