#include "dispatch/DispatchStrategy.h"
#include <set>
#include <stdexcept>

namespace campusfix::dispatch {
DispatchResult DispatchStrategy::recommend(const DispatchWorkOrder &order,
                                          const std::vector<DispatchWorker> &workers) const {
    validateWorkOrder(order);
    std::set<BusinessId> ids;
    for (const auto &worker : workers) {
        validateWorker(worker);
        if (!ids.insert(worker.record.id).second) throw std::invalid_argument("Duplicate dispatch worker");
    }
    auto result = doRecommend(order, workers);
    validateResult(result, workers);
    return result;
}
}
