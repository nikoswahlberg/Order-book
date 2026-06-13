#pragma once

#include <cstdint>
#include <string>

namespace obs {

// Prices are stored as integer "ticks" rather than floating point.
//
// Real matching engines never compare prices with `double` because
// floating-point rounding makes equality unreliable (49.80 might be stored
// as 49.79999999). One tick here is 0.01 EUR, so a price of 49.80 EUR/MWh is
// stored as the integer 4980. Integer comparison is both exact and faster
// than floating-point comparison.
using Price = std::int64_t;   // in ticks (1 tick = 0.01 EUR)
using Quantity = std::int64_t; // in MW (whole units for simplicity)
using OrderId = std::uint64_t;
using Sequence = std::uint64_t; // monotonic counter used for time priority

inline constexpr Price kTicksPerUnit = 100; // 100 ticks = 1.00 EUR

// Convert a human-facing price (e.g. 49.80 EUR/MWh) into integer ticks.
inline constexpr Price to_ticks(double price) {
    // +0.5 for round-half-up on positive prices.
    return static_cast<Price>(price * kTicksPerUnit + (price >= 0 ? 0.5 : -0.5));
}

// Convert integer ticks back into a EUR value for display.
inline constexpr double to_price(Price ticks) {
    return static_cast<double>(ticks) / kTicksPerUnit;
}

enum class Side { Buy, Sell };

inline const char* side_name(Side s) {
    return s == Side::Buy ? "BUY" : "SELL";
}

inline Side opposite(Side s) {
    return s == Side::Buy ? Side::Sell : Side::Buy;
}

} // namespace obs
