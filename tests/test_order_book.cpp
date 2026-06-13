#include "obs/order_book.hpp"
#include "obs/price.hpp"

#include <cstdio>
#include <vector>

using namespace obs;

static int g_failures = 0;
static int g_checks = 0;

#define CHECK(cond)                                                       \
    do {                                                                  \
        ++g_checks;                                                       \
        if (!(cond)) {                                                    \
            std::printf("  FAIL [%s:%d]: %s\n", __FILE__, __LINE__, #cond); \
            ++g_failures;                                                 \
        }                                                                 \
    } while (0)

static void test_price_conversion() {
    CHECK(to_ticks(49.80) == 4980);
    CHECK(to_ticks(50.00) == 5000);
    CHECK(to_price(4980) == 49.80);
}

static void test_simple_match() {
    OrderBook book;
    std::vector<Trade> trades;
    book.add_limit(Side::Buy, to_ticks(50.00), 10, trades);
    CHECK(trades.empty());                    // nothing to cross yet
    CHECK(book.best_bid() == to_ticks(50.00));

    book.add_limit(Side::Sell, to_ticks(50.00), 4, trades);
    CHECK(trades.size() == 1);
    CHECK(trades[0].quantity == 4);
    CHECK(trades[0].price == to_ticks(50.00)); // executes at maker (resting) price
    CHECK(book.best_bid() == to_ticks(50.00)); // 6 MW of the bid remains
}

static void test_partial_fill_rests() {
    OrderBook book;
    std::vector<Trade> trades;
    book.add_limit(Side::Sell, to_ticks(50.00), 5, trades);
    auto rested = book.add_limit(Side::Buy, to_ticks(50.00), 8, trades);
    CHECK(trades.size() == 1);
    CHECK(trades[0].quantity == 5);
    CHECK(rested.has_value());                 // 3 MW left over rests as a bid
    CHECK(book.best_bid() == to_ticks(50.00));
    CHECK(!book.best_ask().has_value());       // ask fully consumed
}

static void test_price_time_priority() {
    OrderBook book;
    std::vector<Trade> trades;
    // Two bids at the same price; the first one in must fill first.
    auto first = book.add_limit(Side::Buy, to_ticks(50.00), 5, trades);
    auto second = book.add_limit(Side::Buy, to_ticks(50.00), 5, trades);
    CHECK(first.has_value() && second.has_value());

    book.add_limit(Side::Sell, to_ticks(50.00), 5, trades);
    CHECK(trades.size() == 1);
    CHECK(trades[0].maker_id == *first);       // earlier order matched first
}

static void test_better_price_first() {
    OrderBook book;
    std::vector<Trade> trades;
    book.add_limit(Side::Sell, to_ticks(50.50), 5, trades);
    book.add_limit(Side::Sell, to_ticks(50.20), 5, trades); // better (lower) ask
    // A buy should hit the lower ask first.
    book.add_limit(Side::Buy, to_ticks(50.50), 5, trades);
    CHECK(trades.size() == 1);
    CHECK(trades[0].price == to_ticks(50.20));
}

static void test_cancel() {
    OrderBook book;
    std::vector<Trade> trades;
    auto id = book.add_limit(Side::Buy, to_ticks(49.00), 10, trades);
    CHECK(id.has_value());
    CHECK(book.resting_orders() == 1);
    CHECK(book.cancel(*id));
    CHECK(book.resting_orders() == 0);
    CHECK(!book.best_bid().has_value());
    CHECK(!book.cancel(*id));                  // double-cancel is a no-op
    // A sell at that price now finds nothing to trade with.
    book.add_limit(Side::Sell, to_ticks(49.00), 10, trades);
    CHECK(trades.empty());
}

static void test_market_sweep() {
    OrderBook book;
    std::vector<Trade> trades;
    book.add_limit(Side::Sell, to_ticks(50.00), 5, trades);
    book.add_limit(Side::Sell, to_ticks(50.20), 5, trades);
    book.add_limit(Side::Sell, to_ticks(50.50), 5, trades);
    Quantity unfilled = book.add_market(Side::Buy, 12, trades);
    CHECK(unfilled == 0);
    CHECK(trades.size() == 3);                 // swept three levels
    CHECK(trades[0].price == to_ticks(50.00)); // best first
    CHECK(trades[2].quantity == 2);            // only 2 MW taken from level 3
}

static void test_market_runs_dry() {
    OrderBook book;
    std::vector<Trade> trades;
    book.add_limit(Side::Sell, to_ticks(50.00), 3, trades);
    Quantity unfilled = book.add_market(Side::Buy, 10, trades);
    CHECK(unfilled == 7);                      // 7 MW left unfilled
    CHECK(!book.best_ask().has_value());
}

static void test_spread() {
    OrderBook book;
    std::vector<Trade> trades;
    CHECK(!book.spread().has_value());
    book.add_limit(Side::Buy, to_ticks(49.80), 5, trades);
    book.add_limit(Side::Sell, to_ticks(50.20), 5, trades);
    CHECK(book.spread() == to_ticks(0.40));
}

int main() {
    test_price_conversion();
    test_simple_match();
    test_partial_fill_rests();
    test_price_time_priority();
    test_better_price_first();
    test_cancel();
    test_market_sweep();
    test_market_runs_dry();
    test_spread();

    std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
    if (g_failures == 0) std::printf("ALL TESTS PASSED\n");
    return g_failures == 0 ? 0 : 1;
}
