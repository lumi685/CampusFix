#pragma once
#include "infrastructure/SqliteConnection.h"
#include <optional>
namespace campusfix {
struct CurrentTaskAssignment {std::int64_t id=0,version=0;std::optional<std::string> acceptedAt;};
class WorkerTaskRepository {
public:
    explicit WorkerTaskRepository(SqliteConnection &db):db_(db){}
    CurrentTaskAssignment current(std::int64_t order,std::int64_t user);
    bool repeatedRefusal(std::int64_t order,std::int64_t user,std::int64_t expectedVersion);
    void change(const CurrentTaskAssignment &,const std::string &utc,bool refuse);
    void supersede(std::int64_t order,std::int64_t assignment,const std::string &utc);
    void advance(std::int64_t order,std::int64_t version,const std::string &target,const std::string &utc);
    bool owner(std::int64_t assignment,std::int64_t user);
private:SqliteConnection &db_;
};
}
