#include "repositories/StatusLogRepository.h"
#include "domain/WorkOrderStateMachine.h"
#include "domain/WorkOrderValidation.h"
#include <stdexcept>

namespace campusfix {
void StatusLogRepository::append(const StatusLogEntry &e) {
    if(e.orderId<=0||e.action.empty()||e.requestId.empty()||!validUtcDateTime(e.createdAt)||
       !WorkOrderStateMachine::knownState(e.toStatus)||(e.fromStatus&&!WorkOrderStateMachine::knownState(*e.fromStatus)))
        throw std::invalid_argument("Invalid status log entry");
    auto q=db_.prepare("INSERT INTO status_logs(order_id,actor_id,assignment_id,action,from_status,to_status,"
        "public_note,internal_note,metadata_json,created_at,request_id) VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11)");
    q.bindInt(1,e.orderId);
    if(e.actorId) q.bindInt(2,*e.actorId);else q.bindNull(2);
    if(e.assignmentId) q.bindInt(3,*e.assignmentId);else q.bindNull(3);
    q.bindText(4,e.action);
    const auto bind=[&](int index,const std::optional<std::string> &v) {if(v) q.bindText(index,*v);else q.bindNull(index);};
    bind(5,e.fromStatus);q.bindText(6,e.toStatus);bind(7,e.publicNote);bind(8,e.internalNote);bind(9,e.metadataJson);
    q.bindText(10,e.createdAt);q.bindText(11,e.requestId);q.step();
}
StatusLogPage StatusLogRepository::page(std::int64_t order,const TimelineQuery &query,bool internal) {
    StatusLogPage result;
    {
        auto q=db_.prepare("SELECT count(*) FROM status_logs WHERE order_id=?1");q.bindInt(1,order);q.step();result.total=q.columnInt(0);
    }
    const auto extra=internal?",s.internal_note,s.metadata_json":"";
    auto q=db_.prepare(std::string("SELECT s.id,s.action,s.from_status,s.to_status,s.public_note,s.created_at,s.actor_id,u.display_name")+
        extra+" FROM status_logs s LEFT JOIN users u ON u.id=s.actor_id WHERE s.order_id=?1 ORDER BY s.created_at ASC,s.id ASC LIMIT ?2 OFFSET ?3");
    q.bindInt(1,order);q.bindInt(2,query.pageSize);q.bindInt(3,(query.page-1)*query.pageSize);
    while(q.step()) {
        StatusLogRecord row;row.id=q.columnInt(0);row.action=q.columnText(1);row.toStatus=q.columnText(3);row.createdAt=q.columnText(5);
        if(!q.columnIsNull(2)) row.fromStatus=q.columnText(2);
        if(!q.columnIsNull(4)) row.publicNote=q.columnText(4);
        if(!q.columnIsNull(6)) {row.actorId=q.columnInt(6);row.actorName=q.columnText(7);}
        if(internal) {
            if(!q.columnIsNull(8)) row.internalNote=q.columnText(8);
            if(!q.columnIsNull(9)) row.metadataJson=q.columnText(9);
        }
        result.items.push_back(std::move(row));
    }
    return result;
}
}
