#pragma once
#include "infrastructure/SqliteConnection.h"
namespace campusfix {
struct VerificationSnapshot {
    std::int64_t assignment=0,assignmentVersion=0,worker=0,reworks=0,load=0;
    std::string summary,accepted,submitted;
};
class VerificationRepository {
public:
    explicit VerificationRepository(SqliteConnection &db):db_(db){}
    VerificationSnapshot read(std::int64_t order);
    void save(std::int64_t order,std::int64_t reporter,std::int64_t version,const VerificationSnapshot &,bool approved,const std::string &utc);
    void end(std::int64_t order,const VerificationSnapshot &,const std::string &utc);
    bool saved(std::int64_t order,std::int64_t reporter,std::int64_t version,const VerificationSnapshot &,bool approved,const std::string &utc);
private: SqliteConnection &db_;
};
}
