#pragma once

#include "obs/price.hpp"

#include <cstdint>
#include <vector>

namespace obs {

enum class EventType { AddLimit, AddMarket, Cancel };

// A single market event. Cancel events reference the n-th AddLimit emitted so
// far (its "ordinal"), because the real OrderId is only known at replay time.
struct Event {
    EventType type;
    Side side;
    Price price;        // for AddLimit
    Quantity qty;       // for AddLimit / AddMarket
    std::uint64_t cancel_ordinal; // for Cancel: which earlier add to target
};

// Parameters controlling the shape of the synthetic order flow.
struct GenConfig {
    std::uint64_t count = 1'000'000; // number of events to generate
    std::uint64_t seed = 42;
    double mid = 50.00;        // starting mid price (EUR/MWh)
    double tick_spread = 0.10; // typical distance of a passive order from mid
    double p_market = 0.05;    // probability an event is a market order
    double p_cancel = 0.25;    // probability an event is a cancel
    double p_aggressive = 0.20; // of limit adds, fraction priced to cross
};

// Pre-generate the full event stream. Generation is kept separate from replay
// so that benchmarks time only the matching engine, not the RNG.
std::vector<Event> generate_events(const GenConfig& cfg);

} // namespace obs
