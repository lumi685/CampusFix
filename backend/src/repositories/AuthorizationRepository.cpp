#include "repositories/AuthorizationRepository.h"
#include "repositories/AuthRepository.h"

namespace campusfix {
Subject AuthorizationRepository::subject(std::int64_t id) {
    auto q=db_.prepare("SELECT id,role,status FROM users WHERE id=?1"); q.bindInt(1,id);
    if(!q.step()) return {};
    Subject s{q.columnInt(0),q.columnText(1),q.columnText(2)=="ACTIVE",{}};
    s.grants=AuthRepository(db_).grants(id);return s;
}
std::vector<std::int64_t> AuthorizationRepository::ancestors(std::int64_t id) {
    // UNION (not UNION ALL) terminates even if invalid cyclic data is present.
    auto q=db_.prepare("WITH RECURSIVE chain(id,parent_id) AS ("
        "SELECT id,parent_id FROM locations WHERE id=?1 UNION "
        "SELECT l.id,l.parent_id FROM locations l JOIN chain c ON l.id=c.parent_id) SELECT id FROM chain");
    q.bindInt(1,id);std::vector<std::int64_t> result;
    while(q.step()) result.push_back(q.columnInt(0));return result;
}
OrderScope AuthorizationRepository::orderScope(std::int64_t orderId,std::int64_t userId) {
    auto q=db_.prepare("SELECT reporter_id,location_id,status,version,"
        "EXISTS(SELECT 1 FROM assignments a JOIN workers w ON a.worker_id=w.id "
        "WHERE a.order_id=o.id AND w.user_id=?2 AND a.ended_at IS NULL),"
        "EXISTS(SELECT 1 FROM assignments a JOIN workers w ON a.worker_id=w.id "
        "WHERE a.order_id=o.id AND w.user_id=?2 AND a.ended_at IS NOT NULL),"
        "EXISTS(SELECT 1 FROM evaluations e WHERE e.order_id=o.id) FROM work_orders o WHERE id=?1");
    q.bindInt(1,orderId);q.bindInt(2,userId);
    if(!q.step()) return {};
    OrderScope o; o.exists=true;o.reporterId=q.columnInt(0);o.status=q.columnText(2);o.version=q.columnInt(3);
    o.currentWorker=q.columnInt(4)!=0;o.historicalWorker=q.columnInt(5)!=0;o.evaluated=q.columnInt(6)!=0;
    o.ancestors=ancestors(q.columnInt(1));return o;
}
std::pair<std::int64_t,std::int64_t> AuthorizationRepository::workerOwnerAndLocation(std::int64_t id) {
    auto q=db_.prepare("SELECT user_id,service_area_id FROM workers WHERE id=?1");q.bindInt(1,id);
    return q.step()?std::make_pair(q.columnInt(0),q.columnInt(1)):std::make_pair(std::int64_t{0},std::int64_t{0});
}
}
