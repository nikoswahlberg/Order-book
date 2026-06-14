#pragma once

#include "obs/order_book.hpp"
#include "data_gen.hpp"

#include <vector>

namespace obs {

// Replays a pre-generated event stream into a book. Maintains the mapping from
// each AddLimit's ordinal to the OrderId that actually rested (0 if it fully
// filled and never rested, in which case the matching cancel is a harmless
// no-op). Returns the total number of trades produced.
class Driver {
public:
    explicit Driver(OrderBook& book) : book_(book) {}

    std::uint64_t replay(const std::vector<Event>& events,
                         std::vector<Trade>& trades) {
        [[maybe_unused]] std::uint64_t add_ordinal = 0;
        for (const Event& e : events) {
            switch (e.type) {
                case EventType::AddLimit: {
                    auto rested =
                        book_.add_limit(e.side, e.price, e.qty, trades);
                    rest_id_.push_back(rested.value_or(0));
                    ++add_ordinal;
                    break;
                }
                case EventType::AddMarket:
                    book_.add_market(e.side, e.qty, trades);
                    break;
                case EventType::Cancel:
                    if (e.cancel_ordinal < rest_id_.size()) {
                        OrderId id = rest_id_[e.cancel_ordinal];
                        if (id != 0) book_.cancel(id);
                    }
                    break;
            }
        }
        return trades.size();
    }

private:
    OrderBook& book_;
    std::vector<OrderId> rest_id_; // ordinal -> resting id (0 = none)
};

} // namespace obs
