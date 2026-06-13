#pragma once

#include "obs/price.hpp"

namespace obs {

// A single resting or incoming order.
struct Order {
    OrderId id;
    Side side;
    Price price;       // limit price in ticks
    Quantity quantity; // remaining quantity in MW
    Sequence seq;      // assigned on arrival; lower seq = higher time priority
};

// A trade (fill) produced when two orders cross.
//
// The trade always executes at the *resting* order's price (the "maker"),
// which is the standard convention: the order that was in the book first
// sets the price, and the incoming aggressor (the "taker") accepts it.
struct Trade {
    OrderId maker_id; // the resting order that was matched against
    OrderId taker_id; // the incoming order that crossed the spread
    Price price;      // execution price in ticks (the maker's price)
    Quantity quantity;
};

} // namespace obs
