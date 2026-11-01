#pragma once
#include "infrastructure/SqliteConnection.h"
#include "repositories/TransferRepository.h"
namespace campusfix {
class CompletionRepository {
public:
    explicit CompletionRepository(SqliteConnection &db):db_(db){}
    void save(std::int64_t order,std::int64_t version,const std::string &summary,const std::string &utc);
    void submit(const TransferAssignment &,std::int64_t order,std::int64_t user,const std::string &utc);
    void supersede(std::int64_t order,std::int64_t assignment,const std::string &utc);
    bool saved(std::int64_t order,std::int64_t version,const TransferAssignment &,const std::string &summary,const std::string &utc);
private: SqliteConnection &db_;
};
}
