#include "repositories/CatalogRepository.h"

namespace campusfix {
std::vector<CatalogItem> CatalogRepository::children(bool locations,std::optional<std::int64_t> parent) {
    // Only fixed server-owned identifiers enter SQL; every request value is bound.
    const std::string table=locations?"locations":"fault_categories";
    const std::string kind=locations?"location_type":"default_priority";
    auto q=db_.prepare("WITH RECURSIVE ancestors(id,parent_id,active) AS ("
        "SELECT id,parent_id,active FROM "+table+" WHERE id=?1 UNION "
        "SELECT p.id,p.parent_id,p.active FROM "+table+" p JOIN ancestors a ON p.id=a.parent_id) "
        "SELECT id,code,name,"+kind+",parent_id FROM "+table+" WHERE active=1 AND "
        "((?1 IS NULL AND parent_id IS NULL) OR (parent_id=?1 "
        "AND EXISTS(SELECT 1 FROM ancestors WHERE id=?1) "
        "AND NOT EXISTS(SELECT 1 FROM ancestors WHERE active<>1))) ORDER BY code,id");
    if(parent) q.bindInt(1,*parent);else q.bindNull(1);
    std::vector<CatalogItem> result;
    while(q.step()) {
        CatalogItem item{q.columnInt(0),q.columnText(1),q.columnText(2),q.columnText(3),{}};
        if(!q.columnIsNull(4)) item.parentId=q.columnInt(4);
        result.push_back(std::move(item));
    }
    return result;
}
}
