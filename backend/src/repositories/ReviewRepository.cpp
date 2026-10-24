#include "repositories/ReviewRepository.h"
#include "models/WorkOrder.h"

namespace campusfix {
void ReviewRepository::validateCategory(std::int64_t id) {
    auto q=db_.prepare("SELECT id FROM fault_categories WHERE id=?1 AND active=1");
    q.bindInt(1,id);
    if(!q.step()) throw WorkOrderError(422,"VALIDATION_ERROR","Invalid active category","category_id");
}
void ReviewRepository::apply(std::int64_t id,const ReviewInput &i,const std::string &target,const std::string &utc) {
    std::string sql="UPDATE work_orders SET status=?1,version=version+1,updated_at=?2";
    if(i.action=="APPROVE") sql+=",category_id=?5,approved_urgency=?6";
    else if(i.action=="REJECT") sql+=",cancelled_at=?2";
    sql+=" WHERE id=?3 AND status='PENDING_REVIEW' AND version=?4";
    auto q=db_.prepare(sql);
    q.bindText(1,target);q.bindText(2,utc);q.bindInt(3,id);q.bindInt(4,i.expectedVersion);
    if(i.action=="APPROVE") {q.bindInt(5,*i.categoryId);q.bindText(6,*i.approvedUrgency);}
    q.step();
    if(sqlite3_changes(db_.nativeHandle())!=1)
        throw WorkOrderError(409,"ORDER_VERSION_CONFLICT","Order changed; reload before reviewing");
}
}
