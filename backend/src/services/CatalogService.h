#pragma once
#include "repositories/CatalogRepository.h"

namespace campusfix {
class CatalogService {
public:
    explicit CatalogService(SqliteConnection &db):db_(db){}
    std::vector<CatalogItem> children(const std::string &token,bool locations,std::optional<std::int64_t> parent);
private: SqliteConnection &db_;
};
}
