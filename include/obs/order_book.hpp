#pragma once

#include "obs/order.hpp"

#include <functional>
#include <list>
#include <map>
#include <optional>
#include <unordered_map>
#include <vector>

namespace obs {

// One price level: a FIFO queue of orders resting at the same price.
// std::list is used (rather than vector) for two reasons:
//   1. Erasing from the middle on cancel is O(1).
//   2. Iterators stay valid when other elements are added/removed, which lets
//      the order index below hold a direct iterator to each resting order.
using PriceLevel = std::list<Order>;

// A snapshot of one aggregated level, for display / market data output.
struct LevelView {
    Price price;
    Quantity quantity;  // total resting quantity at this price
    std::size_t orders; // number of resting orders at this price
};

class OrderBook {
public:
    // Add a limit order. Any quantity that crosses the spread trades
    // immediately (appended to `trades`); any remainder rests in the book.
    // Returns the order id assigned to the resting remainder, or std::nullopt
    // if the order was fully filled and nothing rested.
    std::optional<OrderId> add_limit(Side side, Price price, Quantity qty,
                                     std::vector<Trade>& trades);

    // Add a market order: trades against the best available prices until the
    // quantity is exhausted or the opposite side is empty. Never rests.
    // Returns the quantity that could NOT be filled (0 if fully filled).
    Quantity add_market(Side side, Quantity qty, std::vector<Trade>& trades);

    // Cancel a resting order by id. O(1) average. Returns true if it existed.
    bool cancel(OrderId id);

    // --- Top of book ---------------------------------------------------------
    std::optional<Price> best_bid() const;
    std::optional<Price> best_ask() const;
    std::optional<Price> spread() const; // best_ask - best_bid, if both exist

    // --- Aggregated depth (top `levels` per side) ---------------------------
    std::vector<LevelView> bid_depth(std::size_t levels) const;
    std::vector<LevelView> ask_depth(std::size_t levels) const;

    std::size_t resting_orders() const { return index_.size(); }

private:
    // Where a resting order lives, so cancel() can reach it in O(1).
    struct Location {
        Side side;
        Price price;
        PriceLevel::iterator it;
    };

    // Bids sorted high-to-low so the best (highest) bid is begin().
    std::map<Price, PriceLevel, std::greater<Price>> bids_;
    // Asks sorted low-to-high so the best (lowest) ask is begin().
    std::map<Price, PriceLevel, std::less<Price>> asks_;

    std::unordered_map<OrderId, Location> index_;
    OrderId next_id_ = 1;
    Sequence next_seq_ = 1;

    // Rest the remaining quantity of an order in the book.
    OrderId rest(Side side, Price price, Quantity qty);
};

} // namespace obs
