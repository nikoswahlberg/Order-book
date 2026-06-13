#include "obs/order_book.hpp"

namespace obs {

std::optional<OrderId> OrderBook::add_limit(Side side, Price price,
                                            Quantity qty,
                                            std::vector<Trade>& trades) {
    const OrderId taker_id = next_id_++;
    Quantity remaining = qty;

    if (side == Side::Buy) {
        // A buy crosses while the best ask price is at or below our limit.
        while (remaining > 0 && !asks_.empty()) {
            auto level = asks_.begin();      // best (lowest) ask
            if (level->first > price) break; // no longer crosses
            PriceLevel& queue = level->second;

            // Within a level, fill in FIFO order (time priority).
            while (remaining > 0 && !queue.empty()) {
                Order& maker = queue.front();
                const Quantity fill = std::min(remaining, maker.quantity);
                trades.push_back({maker.id, taker_id, level->first, fill});
                maker.quantity -= fill;
                remaining -= fill;
                if (maker.quantity == 0) {
                    index_.erase(maker.id);
                    queue.pop_front();
                }
            }
            if (queue.empty()) asks_.erase(level);
        }
    } else {
        // A sell crosses while the best bid price is at or above our limit.
        while (remaining > 0 && !bids_.empty()) {
            auto level = bids_.begin();      // best (highest) bid
            if (level->first < price) break; // no longer crosses
            PriceLevel& queue = level->second;

            while (remaining > 0 && !queue.empty()) {
                Order& maker = queue.front();
                const Quantity fill = std::min(remaining, maker.quantity);
                trades.push_back({maker.id, taker_id, level->first, fill});
                maker.quantity -= fill;
                remaining -= fill;
                if (maker.quantity == 0) {
                    index_.erase(maker.id);
                    queue.pop_front();
                }
            }
            if (queue.empty()) bids_.erase(level);
        }
    }

    if (remaining > 0) {
        return rest(side, price, remaining);
    }
    return std::nullopt;
}

Quantity OrderBook::add_market(Side side, Quantity qty,
                               std::vector<Trade>& trades) {
    const OrderId taker_id = next_id_++;
    Quantity remaining = qty;

    if (side == Side::Buy) {
        while (remaining > 0 && !asks_.empty()) {
            auto level = asks_.begin();
            PriceLevel& queue = level->second;
            while (remaining > 0 && !queue.empty()) {
                Order& maker = queue.front();
                const Quantity fill = std::min(remaining, maker.quantity);
                trades.push_back({maker.id, taker_id, level->first, fill});
                maker.quantity -= fill;
                remaining -= fill;
                if (maker.quantity == 0) {
                    index_.erase(maker.id);
                    queue.pop_front();
                }
            }
            if (queue.empty()) asks_.erase(level);
        }
    } else {
        while (remaining > 0 && !bids_.empty()) {
            auto level = bids_.begin();
            PriceLevel& queue = level->second;
            while (remaining > 0 && !queue.empty()) {
                Order& maker = queue.front();
                const Quantity fill = std::min(remaining, maker.quantity);
                trades.push_back({maker.id, taker_id, level->first, fill});
                maker.quantity -= fill;
                remaining -= fill;
                if (maker.quantity == 0) {
                    index_.erase(maker.id);
                    queue.pop_front();
                }
            }
            if (queue.empty()) bids_.erase(level);
        }
    }
    return remaining; // unfilled quantity (market order does not rest)
}

OrderId OrderBook::rest(Side side, Price price, Quantity qty) {
    const OrderId id = next_id_ - 1; // id already consumed by the caller
    Order order{id, side, price, qty, next_seq_++};

    if (side == Side::Buy) {
        PriceLevel& queue = bids_[price];
        queue.push_back(order);
        index_[id] = Location{side, price, std::prev(queue.end())};
    } else {
        PriceLevel& queue = asks_[price];
        queue.push_back(order);
        index_[id] = Location{side, price, std::prev(queue.end())};
    }
    return id;
}

bool OrderBook::cancel(OrderId id) {
    auto found = index_.find(id);
    if (found == index_.end()) return false;

    const Location loc = found->second;
    if (loc.side == Side::Buy) {
        auto level = bids_.find(loc.price);
        level->second.erase(loc.it);
        if (level->second.empty()) bids_.erase(level);
    } else {
        auto level = asks_.find(loc.price);
        level->second.erase(loc.it);
        if (level->second.empty()) asks_.erase(level);
    }
    index_.erase(found);
    return true;
}

std::optional<Price> OrderBook::best_bid() const {
    if (bids_.empty()) return std::nullopt;
    return bids_.begin()->first;
}

std::optional<Price> OrderBook::best_ask() const {
    if (asks_.empty()) return std::nullopt;
    return asks_.begin()->first;
}

std::optional<Price> OrderBook::spread() const {
    if (bids_.empty() || asks_.empty()) return std::nullopt;
    return asks_.begin()->first - bids_.begin()->first;
}

std::vector<LevelView> OrderBook::bid_depth(std::size_t levels) const {
    std::vector<LevelView> out;
    for (const auto& [price, queue] : bids_) {
        if (out.size() >= levels) break;
        Quantity total = 0;
        for (const Order& o : queue) total += o.quantity;
        out.push_back({price, total, queue.size()});
    }
    return out;
}

std::vector<LevelView> OrderBook::ask_depth(std::size_t levels) const {
    std::vector<LevelView> out;
    for (const auto& [price, queue] : asks_) {
        if (out.size() >= levels) break;
        Quantity total = 0;
        for (const Order& o : queue) total += o.quantity;
        out.push_back({price, total, queue.size()});
    }
    return out;
}

} // namespace obs
