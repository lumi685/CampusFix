#pragma once
#include "dispatch/DispatchModels.h"

namespace campusfix::dispatch {
class EligibilityFilter {
public:
    // Pure hard filtering of trusted server snapshots. Invalid structures or
    // unloaded skills throw invalid_argument; unverifiable locations fail closed.
    // Every valid input appears exactly once, sorted by worker ID within each list.
    // No score/rank, cross-area exceptions, database access, or side effects.
    static DispatchResult filter(const DispatchWorkOrder &order,
                                 const std::vector<DispatchWorker> &workers);
};
}
