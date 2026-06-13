#include "data_gen.hpp"

#include <random>

namespace obs {

std::vector<Event> generate_events(const GenConfig& cfg) {
    std::vector<Event> events;
    events.reserve(cfg.count);

    std::mt19937_64 rng(cfg.seed);
    std::uniform_real_distribution<double> unit(0.0, 1.0);
    std::uniform_int_distribution<int> qty_dist(1, 20);     // 1..20 MW
    std::uniform_int_distribution<int> offset_dist(0, 5);   // ticks from mid
    std::normal_distribution<double> drift(0.0, 0.02);      // mid random walk

    double mid = cfg.mid;
    std::uint64_t add_ordinal = 0; // counts AddLimit events emitted

    for (std::uint64_t i = 0; i < cfg.count; ++i) {
        // Let the mid price wander a little so the book keeps moving.
        mid += drift(rng);

        const double roll = unit(rng);
        const Side side = (unit(rng) < 0.5) ? Side::Buy : Side::Sell;

        if (roll < cfg.p_cancel && add_ordinal > 0) {
            // Target some earlier add (favouring recent ones a little).
            std::uniform_int_distribution<std::uint64_t> pick(0, add_ordinal - 1);
            events.push_back({EventType::Cancel, side, 0, 0, pick(rng)});
        } else if (roll < cfg.p_cancel + cfg.p_market) {
            events.push_back({EventType::AddMarket, side, 0,
                              qty_dist(rng), 0});
        } else {
            const double off = (offset_dist(rng) + 1) * cfg.tick_spread;
            double px;
            if (unit(rng) < cfg.p_aggressive) {
                // Aggressive: priced across the mid to trigger a trade.
                px = (side == Side::Buy) ? mid + off : mid - off;
            } else {
                // Passive: priced on its own side of the mid, rests in book.
                px = (side == Side::Buy) ? mid - off : mid + off;
            }
            events.push_back({EventType::AddLimit, side, to_ticks(px),
                              qty_dist(rng), 0});
            ++add_ordinal;
        }
    }
    return events;
}

} // namespace obs
