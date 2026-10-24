#include "ReviewTestData.h"
#include "domain/WorkOrderValidation.h"
#include <future>
#include <atomic>
#include <thread>
#include <iostream>

using namespace campusfix;
int main() {
    initializeCrypto();const auto root=std::filesystem::temp_directory_path()/("campusfix-edit-service-"+newOpaqueId());
    std::filesystem::create_directories(root);int checks=0;
    const auto check=[&](bool ok,const std::string &s){if(!ok)throw std::runtime_error(s);++checks;std::cout<<"PASS SQLite/Service "<<s<<'\n';};
    try {
        const auto path=root/"edit.sqlite3";const auto f=test::readFixture(path,CAMPUSFIX_MIGRATIONS_DIR,CAMPUSFIX_SEED_FILE);
        {
            SqliteConnection db(path);WorkOrderService service(db);const auto reporter=f.accounts.at("demo_reporter01"),dispatcher=f.accounts.at("demo_dispatcher");
            const auto deny=[&](const WorkOrder &o,const WorkOrderInput &i,const test::ReadAccount &a,std::int64_t version,int status) {
                const auto before=test::reviewFingerprint(db);bool rejected=false;
                try {service.edit(a.id,a.token,o.id,version,i,"edit-failure");}catch(const WorkOrderError &e){rejected=e.status==status;}
                check(rejected&&before==test::reviewFingerprint(db),"reject "+std::to_string(status)+" with all business rows/logs unchanged");
            };
            for(const auto *name:{"demo_reporter01","demo_worker01"}) {
                const auto a=f.accounts.at(name);auto o=test::pending(db,f,name);auto r=test::outcome("RETURN");service.review(dispatcher.id,dispatcher.token,o.id,r,"edit-return");
                const auto sequence=db.scalarInt("SELECT sum(last_value) FROM order_number_sequences");const auto count=db.scalarInt("SELECT count(*) FROM work_orders");
                auto input=o.input;input.categoryId=2;input.locationId=12;input.title="Updated fictional title";input.description="PRIVATE_DESCRIPTION_VALUE";
                input.contactName="PRIVATE_CONTACT_VALUE";input.contactPhone="00011122233";input.urgency="HIGH";input.expectedVisitAt="2030-01-01T00:30:00.000Z";
                const auto result=service.edit(a.id,a.token,o.id,3,input,"edit-success");const auto stored=WorkOrderRepository(db).find(o.id);
                check(result.version==4&&result.status=="PENDING_SUPPLEMENT"&&validUtcDateTime(result.updatedAt),"edit preserves state and increments version");
                check(stored.input.categoryId==2&&stored.input.locationId==12&&stored.input.title==input.title&&stored.input.description==input.description&&stored.input.contactName==input.contactName&&stored.input.contactPhone==input.contactPhone&&stored.input.urgency=="HIGH"&&stored.input.expectedVisitAt==input.expectedVisitAt,"all editable fields persisted");
                auto logs=db.prepare("SELECT actor_id,from_status,to_status,public_note,metadata_json,request_id FROM status_logs WHERE order_id=?1 AND action='ORDER_CONTENT_UPDATED'");logs.bindInt(1,o.id);check(logs.step(),"one content update log exists");
                check(logs.columnInt(0)==a.id&&logs.columnText(1)=="PENDING_SUPPLEMENT"&&logs.columnText(2)=="PENDING_SUPPLEMENT"&&logs.columnText(5)=="edit-success"&&!logs.step(),"actor/request and same-state append event");
                auto note=db.prepare("SELECT public_note,metadata_json FROM status_logs WHERE order_id=?1 AND action='ORDER_CONTENT_UPDATED'");note.bindInt(1,o.id);note.step();
                check(note.columnText(0).find(input.description)==std::string::npos&&note.columnText(0).find(input.contactPhone)==std::string::npos&&note.columnIsNull(1),"audit contains field names, not private values or raw JSON");
                const auto submitted=service.submit(a.id,a.token,o.id,4,"edit-resubmit");
                check(submitted.version==5&&submitted.orderNo==o.orderNo&&submitted.submittedAt==o.submittedAt&&submitted.status=="PENDING_REVIEW","same ID/number/first submit; version 2-3-4-5");
                check(db.scalarInt("SELECT sum(last_value) FROM order_number_sequences")==sequence&&db.scalarInt("SELECT count(*) FROM work_orders")==count,"resubmit does not allocate number or create order");
                deny(o,input,a,4,409);
                bool duplicate=false;try {service.submit(a.id,a.token,o.id,5,"duplicate");}catch(const WorkOrderError &e){duplicate=e.status==409;}check(duplicate,"duplicate resubmit rejected");
            }
            auto draft=service.create(reporter.id,reporter.token,{1,11,"Draft title","Draft description","NORMAL","Fictional contact","00000000000",{}},"draft-create");
            auto input=draft.input;input.description="Only description changed";
            const auto edited=service.edit(reporter.id,reporter.token,draft.id,1,input,"draft-edit");
            check(edited.status=="DRAFT"&&edited.version==2&&!WorkOrderRepository(db).find(draft.id).orderNo,"owner edits draft without numbering");
            {auto summary=db.prepare("SELECT public_note FROM status_logs WHERE request_id='draft-edit'");summary.step();check(summary.columnText(0)=="Updated fields: description","summary lists only actual changed field");}
            draft.version=2;
            for(const auto *name:{"demo_reporter02","demo_worker01","demo_dispatcher","demo_sysadmin"}) deny(draft,input,f.accounts.at(name),2,std::string(name)=="demo_dispatcher"?403:404);
            deny({f.current},input,f.accounts.at("demo_worker01"),1,403);
            auto invalid=reporter;invalid.token="invalid-token";deny(draft,input,invalid,2,401);
            deny(draft,input,reporter,1,409);
            for(const auto *status:{"PENDING_REVIEW","PENDING_DISPATCH","PENDING_ACCEPT","IN_PROGRESS","PENDING_VERIFY","COMPLETED","CLOSED","CANCELLED"}) {
                auto q=db.prepare("UPDATE work_orders SET status=?1 WHERE id=?2");q.bindText(1,status);q.bindInt(2,draft.id);q.step();deny(draft,input,reporter,2,409);
            }
            {auto q=db.prepare("UPDATE work_orders SET status='DRAFT' WHERE id=?1");q.bindInt(1,draft.id);q.step();}
            for(const auto max:{120,2000,80,32}) {
                auto i=input;std::string *field=max==120?&i.title:max==2000?&i.description:max==80?&i.contactName:&i.contactPhone;
                *field=std::string(static_cast<std::size_t>(max+1),'x');deny(draft,i,reporter,2,422);
                const auto boundary=service.create(reporter.id,reporter.token,draft.input,"boundary-draft");
                field->assign(static_cast<std::size_t>(max),'x');
                check(service.edit(reporter.id,reporter.token,boundary.id,1,i,"boundary-edit").version==2,"maximum valid editable string length "+std::to_string(max));
            }
            for(const auto &value:{std::string(),std::string(" \t"),std::string("a\0b",3),std::string("\xC0\x80"),std::string("a\x01",2)}) {auto i=input;i.title=value;deny(draft,i,reporter,2,422);}
            for(const auto field:{"category","location","urgency","date"}) {auto i=input;const std::string s=field;if(s=="category")i.categoryId=999999;else if(s=="location")i.locationId=999999;else if(s=="urgency")i.urgency="INVALID";else i.expectedVisitAt="2030-02-30T00:00:00Z";deny(draft,i,reporter,2,422);}
            for(const auto *table:{"locations","fault_categories"}) {const std::string where=std::string(table)=="locations"?"11":"1";db.execute(std::string("UPDATE ")+table+" SET active=0 WHERE id="+where);deny(draft,input,reporter,2,422);db.execute(std::string("UPDATE ")+table+" SET active=1 WHERE id="+where);}
            db.execute("UPDATE locations SET active=0 WHERE id=1");deny(draft,input,reporter,2,422);db.execute("UPDATE locations SET active=1 WHERE id=1");
            db.execute("UPDATE fault_categories SET parent_id=2 WHERE id=1");db.execute("UPDATE fault_categories SET active=0 WHERE id=2");deny(draft,input,reporter,2,422);db.execute("UPDATE fault_categories SET active=1 WHERE id=2");db.execute("UPDATE fault_categories SET parent_id=NULL WHERE id=1");
            for(const auto *sql:{"CREATE TRIGGER fail_edit BEFORE INSERT ON status_logs WHEN NEW.action='ORDER_CONTENT_UPDATED' BEGIN SELECT RAISE(ABORT,'test edit log'); END","CREATE TRIGGER fail_edit AFTER UPDATE OF description ON work_orders BEGIN SELECT RAISE(ABORT,'test edit update'); END"}) {
                db.execute(sql);const auto before=test::reviewFingerprint(db);bool failed=false;try {service.edit(reporter.id,reporter.token,draft.id,2,input,"failed-edit");}catch(const SqliteError &){failed=true;}
                check(failed&&before==test::reviewFingerprint(db),"injected edit failure rolls back body and audit");db.execute("DROP TRIGGER fail_edit");
            }
            std::atomic<int> ready{0};const auto race=[&](){SqliteConnection connection(path);++ready;while(ready<2)std::this_thread::yield();try {WorkOrderService(connection).edit(reporter.id,reporter.token,draft.id,2,input,"race-edit");return 200;}catch(const WorkOrderError &e){return e.status;}};
            auto first=std::async(std::launch::async,race),second=std::async(std::launch::async,race);const auto a=first.get(),b=second.get();
            check((a==200&&b==409)||(a==409&&b==200),"two-connection edits produce one success and one 409");
            test::integrity(db);check(true,"integrity_check=ok; foreign_key_check empty");
        }
        std::filesystem::remove_all(root);std::cout<<"PASS "<<checks<<" edit SQLite/Service assertions\n";return 0;
    }catch(const std::exception &e){std::cerr<<e.what()<<'\n';std::filesystem::remove_all(root);return 1;}
}
