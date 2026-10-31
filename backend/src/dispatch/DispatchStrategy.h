#pragma once
#include "dispatch/DispatchModels.h"

namespace campusfix::dispatch {
class DispatchStrategy {
public:
    virtual ~DispatchStrategy() = default;
    // Implementations must be deterministic, input-preserving and free of I/O.
    // Guards validate shape only. Future production implementations must execute
    // hard filtering BEFORE scoring; this interface does not replace that work.
    DispatchResult recommend(const DispatchWorkOrder &order,
                             const std::vector<DispatchWorker> &workers) const;
protected:
    virtual DispatchResult doRecommend(const DispatchWorkOrder &order,
                                      const std::vector<DispatchWorker> &workers) const = 0;
};
}
