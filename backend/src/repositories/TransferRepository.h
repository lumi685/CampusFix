#pragma once
#include "infrastructure/SqliteConnection.h"
#include "models/Transfer.h"
namespace campusfix {
struct TransferRecord {std::int64_t id=0,assignmentId=0;std::string status;};
struct TransferAssignment {std::int64_t id=0,version=0,userId=0;std::optional<std::string> acceptedAt;};
class TransferRepository {
public:
    explicit TransferRepository(SqliteConnection &db):db_(db){}
    TransferPage list(std::int64_t user,const std::string &capability,const TransferQuery &query);
    TransferAssignment current(std::int64_t orderId);
    TransferRecord find(std::int64_t orderId,std::int64_t requestId);
    std::optional<std::int64_t> pending(std::int64_t assignmentId);
    std::int64_t insert(std::int64_t order,std::int64_t assignment,std::int64_t user,const std::string &reason,std::int64_t version,const std::string &utc);
    void resolve(std::int64_t request,std::int64_t assignment,std::int64_t user,const std::string &decision,const std::string &reason,const std::string &utc);
    void end(const TransferAssignment &assignment,const std::string &utc);
    void advance(std::int64_t order,std::int64_t version,const std::string &from,const std::string &to,const std::string &utc);
    bool arrived(std::int64_t assignment);
private:
    SqliteConnection &db_;
};
}
