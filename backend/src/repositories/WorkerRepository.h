#pragma once
#include "models/Worker.h"
#include "infrastructure/SqliteConnection.h"

namespace campusfix {
class WorkerRepository {
public:
    explicit WorkerRepository(SqliteConnection &db):db_(db){}
    WorkerPage list(std::int64_t user,const std::string &capability,const WorkerQuery &query);
    WorkerRecord find(std::int64_t id);
    void validateReferences(const WorkerInput &input);
    void update(std::int64_t id,const WorkerInput &input,const std::string &utc);
private:
    SqliteConnection &db_;
    std::vector<WorkerSkill> skills(std::int64_t id);
};
}
