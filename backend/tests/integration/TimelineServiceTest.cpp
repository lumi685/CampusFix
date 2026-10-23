#include "TimelineTestData.h"
#include "services/WorkOrderService.h"
#include "domain/WorkOrderStateMachine.h"
#include <algorithm>
#include <iostream>
#include <limits>
#include <set>

using namespace campusfix;
namespace {
int checks=0;
void check(bool ok,const char *name) {if(!ok) throw std::runtime_error(name);++checks;std::cout<<"PASS "<<name<<'\n';}
template<class F> void rejects(F f,int status,const std::string &code) {
    try {f();}catch(const WorkOrderError &e){check(e.status==status&&e.code==code,"expected timeline rejection");return;}
    throw std::runtime_error("Expected rejection");
}
}
int main() {
    initializeCrypto();const auto root=std::filesystem::temp_directory_path()/("campusfix-timeline-"+newOpaqueId());
    try {
        std::filesystem::create_directories(root);const auto path=root/"timeline.sqlite3";
        const auto f=test::timelineFixture(path,CAMPUSFIX_MIGRATIONS_DIR,CAMPUSFIX_SEED_FILE);
        {
        SqliteConnection db(path);WorkOrderService service(db);const auto baseline=test::timelineFingerprint(db);
        auto timeline=[&](const char *name,std::int64_t id,TimelineQuery query=TimelineQuery{}) {
            const auto &a=f.base.accounts.at(name);return service.timeline(a.id,a.token,id,query);
        };
        const auto publicPage=timeline("demo_reporter01",f.base.current);
        check(publicPage.total==f.total&&publicPage.page==1&&publicPage.pageSize==20,"default timeline paging and actual total");
        std::int64_t previous=0;
        for(const auto &item:publicPage.items) {
            check(!item.internalNote&&item.managementDetail.empty(),"public DTO excludes internal/raw metadata");
            check(item.id>previous,"equal-time sort uses ascending id");previous=item.id;
        }
        check(timeline("demo_worker01",4).total==2,"worker own report timeline");
        check(timeline("demo_worker01",f.base.current).total==f.total,"current worker timeline");
        const auto history=timeline("demo_worker01",f.base.history);
        check(history.total==1&&!history.items.front().internalNote&&history.items.front().managementDetail.empty(),"historical task timeline has public fields only, not internal notes");
        const auto empty=timeline("demo_reporter01",f.base.draft);check(empty.total==0&&empty.items.empty(),"authorized empty legacy-fixture timeline");
        rejects([&]{timeline("demo_reporter02",f.base.current);},404,"RESOURCE_NOT_FOUND");
        rejects([&]{timeline("demo_worker01",f.base.foreign);},404,"RESOURCE_NOT_FOUND");
        rejects([&]{timeline("demo_dispatcher",f.base.foreign);},404,"RESOURCE_NOT_FOUND");
        rejects([&]{timeline("test_no_grants",f.base.current);},404,"RESOURCE_NOT_FOUND");
        rejects([&]{timeline("demo_reporter01",999999);},404,"RESOURCE_NOT_FOUND");
        rejects([&]{timeline("demo_reporter01",0);},422,"VALIDATION_ERROR");
        auto manager=timeline("demo_dispatcher",f.base.current);
        check(std::any_of(manager.items.begin(),manager.items.end(),[](const TimelineItem &i){return i.internalNote.has_value();}),"in-scope dispatcher receives internal notes");
        int goodReview=0,goodAssign=0,goodTransfer=0,goodRework=0,systems=0;
        for(const auto &item:manager.items) {
            if(!item.actorId) ++systems;
            if(item.managementDetail.empty()) continue;
            std::set<std::string> keys;for(const auto &e:item.managementDetail) keys.insert(e.first);
            if(item.action=="ORDER_REVIEWED") {check(keys==std::set<std::string>{"review_action","category_id","approved_urgency"},"review-only management whitelist");++goodReview;}
            else if(item.action=="ORDER_ASSIGNED") {check(keys==std::set<std::string>{"assignment_id","worker_id","candidate_rank","assign_reason"},"assignment-only whitelist with scalar types");++goodAssign;}
            else if(item.action=="TRANSFER_APPROVED") {check(keys==std::set<std::string>{"transfer_request_id","decision"},"transfer-only whitelist");++goodTransfer;}
            else if(item.action=="ORDER_REWORK_REQUESTED") {check(keys==std::set<std::string>{"rework_count"},"rework-only whitelist");++goodRework;}
            else throw std::runtime_error("Unknown event exposed metadata");
        }
        check(goodReview==1&&goodAssign==1&&goodTransfer==1&&goodRework==1&&systems==1,"malformed, unknown, cross-type and deep metadata fail closed; null actor supported");
        check(timeline("demo_sysadmin",f.base.foreign).items.front().internalNote.has_value(),"matching initial B grants permit B internal fields");
        test::timelineGrantMode(db,"admin-mismatch");
        check(!timeline("demo_sysadmin",f.base.current).items.front().internalNote,"separate capability scopes must both match");
        test::timelineGrantMode(db,"admin-match");check(timeline("demo_sysadmin",f.base.current).items.front().internalNote.has_value(),"building BUSINESS_READ plus room INTERNAL_NOTE_READ covers descendant");
        test::timelineGrantMode(db,"revoke-internal");
        for(const auto &item:timeline("demo_sysadmin",f.base.current).items) check(!item.internalNote&&item.managementDetail.empty(),"revoking internal permission takes effect on next call");
        test::timelineGrantMode(db,"revoke-business");rejects([&]{timeline("demo_sysadmin",f.base.current);},404,"RESOURCE_NOT_FOUND");
        test::timelineGrantMode(db,"revoke-dispatcher");rejects([&]{timeline("demo_dispatcher",f.base.current);},404,"RESOURCE_NOT_FOUND");
        auto fifty=timeline("demo_reporter01",f.base.current,{1,50}),hundred=timeline("demo_reporter01",f.base.current,{1,100});
        check(fifty.total==f.total&&hundred.items.size()==static_cast<std::size_t>(f.total),"explicit page sizes 50 and 100");
        std::vector<std::int64_t> paged;
        for(std::int64_t n=1;n<=f.total;++n) {const auto p=timeline("demo_reporter01",f.base.current,{n,1});check(p.items.size()==1&&p.total==f.total,"single-item pagination total");paged.push_back(p.items.front().id);}
        check(std::is_sorted(paged.begin(),paged.end())&&std::set<std::int64_t>(paged.begin(),paged.end()).size()==paged.size(),"pagination no duplicates or missing events");
        check(timeline("demo_reporter01",f.base.current,{f.total+1,1}).items.empty(),"past last timeline page");
        for(const auto &q:std::vector<TimelineQuery>{{0,20},{1,0},{1,101},{std::numeric_limits<std::int64_t>::max(),100}})
            rejects([&]{timeline("demo_reporter01",f.base.current,q);},422,"VALIDATION_ERROR");
        const auto changes=sqlite3_total_changes64(db.nativeHandle());
        timeline("demo_reporter01",f.base.current);WorkOrderStateMachine::find("PENDING_DISPATCH",WorkflowAction::Recommend);
        check(sqlite3_total_changes64(db.nativeHandle())==changes&&test::timelineFingerprint(db)==baseline,"GET/readonly recommendation definition changes no state, version or log row");
        // The append repository must join the caller's transaction and never commit on its own.
        const auto before=db.scalarInt("SELECT count(*) FROM status_logs");
        {
            SqliteTransaction tx(db);StatusLogEntry e;e.orderId=f.base.current;e.action="TEST_APPEND";e.fromStatus="IN_PROGRESS";
            e.toStatus="IN_PROGRESS";e.createdAt="2020-02-01T11:00:00.000Z";e.requestId="rollback-fixture";
            StatusLogRepository(db).append(e);
        }
        check(db.scalarInt("SELECT count(*) FROM status_logs")==before&&test::timelineFingerprint(db)==baseline,"outer transaction rollback removes uncommitted append without changing old history");
        check(db.scalarInt("SELECT count(*) FROM pragma_foreign_key_check")==0,"timeline fixture foreign_key_check empty");
        auto integrity=db.prepare("PRAGMA integrity_check");check(integrity.step()&&integrity.columnText(0)=="ok","timeline fixture integrity_check ok");
        }
        std::filesystem::remove_all(root);std::cout<<"PASS "<<checks<<" timeline SQLite/Service checks (not HTTP)\n";return 0;
    }catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';std::filesystem::remove_all(root);return 1;}
}
