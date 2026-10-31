#pragma once
#include "repositories/DispatchRepository.h"
#include "models/WorkOrder.h"
#include <json/json.h>

namespace campusfix {
class DispatchError : public WorkOrderError {
public:
    DispatchError(int httpStatus,std::string businessCode,std::string message,std::string reasonValue={})
        :WorkOrderError(httpStatus,std::move(businessCode),std::move(message)),reason(std::move(reasonValue)){}
    std::string reason;
};
// Shared whitelist for HTTP preview and the transaction's final snapshot.
Json::Value dispatchResponse(const DispatchData &,const dispatch::DispatchResult &);
std::string exclusionCode(dispatch::ExclusionReason);
void authenticateDispatch(SqliteConnection &,std::int64_t user,const std::string &token);
class DispatchService {
public:
    explicit DispatchService(SqliteConnection &db):db_(db){}
    Json::Value recommend(std::int64_t user,const std::string &token,std::int64_t orderId);
private:
    SqliteConnection &db_;
};
}
