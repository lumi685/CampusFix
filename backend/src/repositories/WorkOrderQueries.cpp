#include "repositories/WorkOrderRepository.h"

namespace campusfix {
namespace {
const std::string summaryColumns=
    "o.id,o.order_no,o.title,o.requested_urgency,o.approved_urgency,o.status,o.version,"
    "o.created_at,o.submitted_at,o.updated_at,c.id,c.code,c.name,l.id,l.code,l.name";
const std::string summaryFrom=" FROM work_orders o JOIN fault_categories c ON c.id=o.category_id JOIN locations l ON l.id=o.location_id ";
OrderSummary readSummary(SqliteStatement &q) {
    OrderSummary s;s.id=q.columnInt(0);s.title=q.columnText(2);s.urgency=q.columnText(3);
    if(!q.columnIsNull(1)) s.orderNo=q.columnText(1);
    if(!q.columnIsNull(4)) s.approvedUrgency=q.columnText(4);
    s.status=q.columnText(5);s.version=q.columnInt(6);s.createdAt=q.columnText(7);
    if(!q.columnIsNull(8)) s.submittedAt=q.columnText(8);
    s.updatedAt=q.columnText(9);
    s.category={q.columnInt(10),q.columnText(11),q.columnText(12)};
    s.location={q.columnInt(13),q.columnText(14),q.columnText(15)};
    return s;
}
}
OrderPage WorkOrderRepository::list(std::int64_t user,const std::string &cap,const OrderQuery &query) {
    // The enum and capability are chosen by the authenticated service, never raw query SQL.
    // UNION terminates malformed cycles and overlapping grants cannot duplicate orders.
    const std::string cte=query.view==OrderView::Managed?
        "WITH RECURSIVE allowed_locations(id) AS ("
        "SELECT l.id FROM locations l JOIN management_grants g ON g.scope_location_id=l.id "
        "WHERE g.user_id=?1 AND g.capability=?2 AND g.revoked_at IS NULL UNION "
        "SELECT l.id FROM locations l JOIN allowed_locations a ON l.parent_id=a.id) ":"";
    std::string scope;
    switch(query.view) {
    case OrderView::Reported:scope="o.reporter_id=?1";break;
    case OrderView::Assigned:scope="EXISTS(SELECT 1 FROM assignments a JOIN workers w ON w.id=a.worker_id WHERE a.order_id=o.id AND w.user_id=?1"+std::string(query.currentOnly?" AND a.ended_at IS NULL":"")+")";break;
    case OrderView::Managed:scope="(o.location_id IN (SELECT id FROM allowed_locations) OR EXISTS("
        "SELECT 1 FROM management_grants g WHERE g.user_id=?1 AND g.capability=?2 AND g.revoked_at IS NULL AND g.scope_location_id IS NULL))";break;
    }
    const std::string where=" WHERE "+scope+" AND (?3 IS NULL OR o.status=?3) AND (?4 IS NULL OR o.category_id=?4)";
    auto bind=[&](SqliteStatement &q) {
        q.bindInt(1,user);
        // ?2 is intentionally unused by non-management predicates, but part of the numbered parameter range.
        q.bindText(2,cap);
        if(query.status) q.bindText(3,*query.status);else q.bindNull(3);
        if(query.categoryId) q.bindInt(4,*query.categoryId);else q.bindNull(4);
    };
    OrderPage result;result.page=query.page;result.pageSize=query.pageSize;
    {
        auto count=db_.prepare(cte+"SELECT count(*)"+summaryFrom+where);bind(count);count.step();result.total=count.columnInt(0);
    }
    auto rows=db_.prepare(cte+"SELECT "+summaryColumns+summaryFrom+where+" ORDER BY o.created_at DESC,o.id DESC LIMIT ?5 OFFSET ?6");
    bind(rows);rows.bindInt(5,query.pageSize);rows.bindInt(6,(query.page-1)*query.pageSize);
    while(rows.step()) result.items.push_back(readSummary(rows));
    return result;
}
OrderDetail WorkOrderRepository::detail(std::int64_t id,bool includeContact) {
    const std::string extras=includeContact?",o.description,o.contact_name,o.contact_phone,o.expected_visit_at":"";
    auto q=db_.prepare("SELECT "+summaryColumns+extras+summaryFrom+" WHERE o.id=?1");q.bindInt(1,id);
    if(!q.step()) throw WorkOrderError(404,"RESOURCE_NOT_FOUND","Work order not found");
    OrderDetail result;result.summary=readSummary(q);
    if(includeContact) {
        OrderContact contact{q.columnText(16),q.columnText(17),q.columnText(18),{}};
        if(!q.columnIsNull(19)) contact.expectedVisitAt=q.columnText(19);
        result.contact=std::move(contact);
        auto assignment=db_.prepare("SELECT w.id,u.display_name FROM assignments a JOIN workers w ON w.id=a.worker_id "
            "JOIN users u ON u.id=w.user_id WHERE a.order_id=?1 AND a.ended_at IS NULL");
        assignment.bindInt(1,id);
        if(assignment.step()) result.currentAssignment=OrderAssignment{assignment.columnInt(0),assignment.columnText(1)};
    }
    // Do not load event/internal JSON or attachment paths. Attachment APIs are a later stage.
    return result;
}
}
