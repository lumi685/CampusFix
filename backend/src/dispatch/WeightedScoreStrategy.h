#pragma once
#include "dispatch/DispatchStrategy.h"

namespace campusfix::dispatch {
class WeightedScoreStrategy final : public DispatchStrategy {
protected:
    // First runs the real EligibilityFilter. Only eligible workers need BOTH
    // histories loaded, including explicitly loaded empty vectors for cold start.
    // The server adapter must supply the latest <=20 valid samples per metric,
    // with final-completion ownership/timestamps per frozen database design 6.5.
    // No database reads, sample selection or fabricated history in this strategy.
    // Components and total are one-decimal scores; total is rounded from returned
    // components and p0Weights. Sort that total, then activeTasks, then workerId.
    DispatchResult doRecommend(const DispatchWorkOrder &order,
                               const std::vector<DispatchWorker> &workers) const override;
};
}
