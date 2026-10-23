#pragma once
#include "infrastructure/SqliteConnection.h"
#include <optional>
#include <vector>

namespace campusfix {
struct CatalogItem {
    std::int64_t id=0;
    std::string code,name,kind;
    std::optional<std::int64_t> parentId;
};
class CatalogRepository {
public:
    explicit CatalogRepository(SqliteConnection &db):db_(db){}
    std::vector<CatalogItem> children(bool locations,std::optional<std::int64_t> parent);
private: SqliteConnection &db_;
};
}
