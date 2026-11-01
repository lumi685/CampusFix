#pragma once
#include "repositories/TransferRepository.h"
namespace campusfix {
// Dedicated connection: list enables query_only and never restores write access.
class TransferQueryService {
public:
    explicit TransferQueryService(SqliteConnection &db):db_(db){}
    TransferPage list(std::int64_t user,const std::string &token,const TransferQuery &query);
private:SqliteConnection &db_;
};
}
