#pragma once
#include "WorkerTaskTestData.h"
#include "services/CompletionService.h"
#include "services/VerificationService.h"
#include "services/EvaluationService.h"
#include "domain/WorkOrderValidation.h"

namespace campusfix::test {
// Positive fixture preconditions use the real Services, never SQL status jumps.
inline std::int64_t completedOrder(SqliteConnection &db,const ReadData &f,const std::string &owner="demo_reporter01") {
    const auto p=pending(db,f,owner);auto approval=approve();approval.categoryId=1;
    const auto a=f.accounts.at("demo_dispatcher"),w=f.accounts.at("demo_worker01"),r=f.accounts.at(owner);
    WorkOrderService(db).review(a.id,a.token,p.id,approval,"eval-review");
    AssignmentService(db).assign(a.id,a.token,p.id,{1,3,std::string("Fictional repair")},"eval-assign");
    taskAction(db,w,p.id);CompletionService(db).complete(w.id,w.token,p.id,5,"Fictional result","eval-complete");
    VerificationService(db).verify(r.id,r.token,p.id,6,true,{},"eval-verify");return p.id;
}
}
