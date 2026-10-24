#pragma once
#include "infrastructure/SqliteConnection.h"
#include "models/WorkOrderReview.h"

namespace campusfix {
class ReviewRepository {
public:
    explicit ReviewRepository(SqliteConnection &db):db_(db){}
    void validateCategory(std::int64_t id);
    // Caller owns the transaction and authorization; every mutation checks status and version.
    void apply(std::int64_t id,const ReviewInput &input,const std::string &target,const std::string &utc);
private:
    SqliteConnection &db_;
};
}
