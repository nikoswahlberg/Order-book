//
// web/bindings.cpp
//
// This file is the glue between C++ and JavaScript. It is compiled only by
// Emscripten (not by the regular Makefile). It uses Emscripten's "embind"
// library to export C++ classes and functions so that JavaScript can call them
// directly — no HTTP server, no subprocess, no serialisation overhead. The
// matching engine runs entirely inside a WebAssembly module in the browser.
//
// Build command (run on your Mac after `brew install emscripten`):
//
//   cd web
//   emcc -std=c++20 -O2 \
//        -I../include -I../src \
//        bindings.cpp ../src/order_book.cpp ../src/data_gen.cpp \
//        -lembind \
//        -s MODULARIZE=1 \
//        -s EXPORT_NAME=OrderBookModule \
//        -s ALLOW_MEMORY_GROWTH=1 \
//        -o order_book.js
//
// This produces two files: order_book.js (JS loader) and order_book.wasm.
// Place both next to index.html and open index.html in a browser.
//

#include "obs/order_book.hpp"
#include "data_gen.hpp"
#include "driver.hpp"

#include <emscripten/bind.h>
#include <vector>
#include <string>
#include <sstream>

using namespace obs;
using namespace emscripten;

// ---------------------------------------------------------------------------
// Wrapper structs that are simple enough for embind to expose to JS.
// ---------------------------------------------------------------------------

struct JSLevelView {
    double price;
    int    quantity;
    int    orders;
};

struct JSTrade {
    int    maker_id;
    int    taker_id;
    double price;
    int    quantity;
};

struct JSBookSnapshot {
    std::vector<JSLevelView> bids;
    std::vector<JSLevelView> asks;
    double best_bid;   // -1 if absent
    double best_ask;   // -1 if absent
    double spread;     // -1 if absent
    int    resting;
};

struct JSSimResult {
    JSBookSnapshot snapshot;
    std::vector<JSTrade> trades;
    int total_trades;
    int total_volume;
};

struct JSBenchResult {
    int    events;
    int    trades;
    double wall_ms;
    double throughput;   // events per second
    double p50_ns;
    double p90_ns;
    double p99_ns;
    double p999_ns;
};

// ---------------------------------------------------------------------------
// Helper: convert internal types to JS-friendly structs
// ---------------------------------------------------------------------------

static JSLevelView to_js(const LevelView& lv) {
    return {to_price(lv.price), static_cast<int>(lv.quantity),
            static_cast<int>(lv.orders)};
}

static JSTrade to_js(const Trade& t) {
    return {static_cast<int>(t.maker_id), static_cast<int>(t.taker_id),
            to_price(t.price), static_cast<int>(t.quantity)};
}

static JSBookSnapshot snap(const OrderBook& book, int levels = 8) {
    JSBookSnapshot s;
    for (const auto& lv : book.bid_depth(levels)) s.bids.push_back(to_js(lv));
    for (const auto& lv : book.ask_depth(levels)) s.asks.push_back(to_js(lv));
    s.best_bid = book.best_bid() ? to_price(*book.best_bid()) : -1.0;
    s.best_ask = book.best_ask() ? to_price(*book.best_ask()) : -1.0;
    s.spread   = book.spread()   ? to_price(*book.spread())   : -1.0;
    s.resting  = static_cast<int>(book.resting_orders());
    return s;
}

// ---------------------------------------------------------------------------
// Exported class: a stateful order book the JS page can interact with
// step by step (for the live demo panel).
// ---------------------------------------------------------------------------

class JSOrderBook {
public:
    JSOrderBook() = default;

    // Place a limit order. Returns JSON string with trade list for simplicity.
    std::string add_limit(bool is_buy, double price, int qty) {
        trades_.clear();
        book_.add_limit(is_buy ? Side::Buy : Side::Sell,
                        to_ticks(price), qty, trades_);
        return trades_json();
    }

    std::string add_market(bool is_buy, int qty) {
        trades_.clear();
        book_.add_market(is_buy ? Side::Buy : Side::Sell, qty, trades_);
        return trades_json();
    }

    bool cancel(int id) { return book_.cancel(static_cast<OrderId>(id)); }

    // Returns a JSON string describing the current book depth.
    std::string get_snapshot(int levels) {
        auto s = snap(book_, levels);
        std::ostringstream o;
        o << "{\"bids\":[";
        for (std::size_t i = 0; i < s.bids.size(); ++i) {
            if (i) o << ",";
            o << "{\"price\":" << s.bids[i].price
              << ",\"quantity\":" << s.bids[i].quantity
              << ",\"orders\":"  << s.bids[i].orders << "}";
        }
        o << "],\"asks\":[";
        for (std::size_t i = 0; i < s.asks.size(); ++i) {
            if (i) o << ",";
            o << "{\"price\":" << s.asks[i].price
              << ",\"quantity\":" << s.asks[i].quantity
              << ",\"orders\":"  << s.asks[i].orders << "}";
        }
        o << "]"
          << ",\"best_bid\":" << s.best_bid
          << ",\"best_ask\":" << s.best_ask
          << ",\"spread\":"   << s.spread
          << ",\"resting\":"  << s.resting
          << "}";
        return o.str();
    }

    void reset() { book_ = OrderBook(); trades_.clear(); }

private:
    OrderBook book_;
    std::vector<Trade> trades_;

    std::string trades_json() {
        std::ostringstream o;
        o << "[";
        for (std::size_t i = 0; i < trades_.size(); ++i) {
            if (i) o << ",";
            o << "{\"maker_id\":" << trades_[i].maker_id
              << ",\"taker_id\":" << trades_[i].taker_id
              << ",\"price\":"    << to_price(trades_[i].price)
              << ",\"quantity\":" << trades_[i].quantity << "}";
        }
        o << "]";
        return o.str();
    }
};

// ---------------------------------------------------------------------------
// Exported function: run a full simulation and return stats + final snapshot.
// Used by the "Run Simulation" panel.
// ---------------------------------------------------------------------------

std::string run_simulation(int event_count, int seed,
                           double p_market, double p_cancel,
                           double p_aggressive) {
    GenConfig cfg;
    cfg.count        = static_cast<std::uint64_t>(event_count);
    cfg.seed         = static_cast<std::uint64_t>(seed);
    cfg.p_market     = p_market;
    cfg.p_cancel     = p_cancel;
    cfg.p_aggressive = p_aggressive;

    auto events = generate_events(cfg);
    OrderBook book;
    std::vector<Trade> trades;
    trades.reserve(events.size());
    Driver driver(book);
    driver.replay(events, trades);

    int total_vol = 0;
    for (const auto& t : trades) total_vol += static_cast<int>(t.quantity);

    auto s = snap(book, 8);
    std::ostringstream o;
    o << "{\"total_trades\":" << trades.size()
      << ",\"total_volume\":" << total_vol
      << ",\"resting\":"      << s.resting
      << ",\"best_bid\":"     << s.best_bid
      << ",\"best_ask\":"     << s.best_ask
      << ",\"spread\":"       << s.spread
      << ",\"bids\":[";
    for (std::size_t i = 0; i < s.bids.size(); ++i) {
        if (i) o << ",";
        o << "{\"price\":" << s.bids[i].price
          << ",\"quantity\":" << s.bids[i].quantity
          << ",\"orders\":"  << s.bids[i].orders << "}";
    }
    o << "],\"asks\":[";
    for (std::size_t i = 0; i < s.asks.size(); ++i) {
        if (i) o << ",";
        o << "{\"price\":" << s.asks[i].price
          << ",\"quantity\":" << s.asks[i].quantity
          << ",\"orders\":"  << s.asks[i].orders << "}";
    }
    o << "]}";
    return o.str();
}

// ---------------------------------------------------------------------------
// Exported function: benchmark — returns throughput + latency percentiles.
// ---------------------------------------------------------------------------

std::string run_benchmark(int event_count) {
    GenConfig cfg;
    cfg.count = static_cast<std::uint64_t>(event_count);
    auto events = generate_events(cfg);

    OrderBook book;
    std::vector<Trade> trades;
    trades.reserve(events.size());
    std::vector<double> lat;
    lat.reserve(events.size());
    std::vector<OrderId> rest_id;
    rest_id.reserve(events.size());

    using Clock = std::chrono::steady_clock;
    auto wall_start = Clock::now();

    for (const auto& e : events) {
        auto t0 = Clock::now();
        switch (e.type) {
            case EventType::AddLimit: {
                auto r = book.add_limit(e.side, e.price, e.qty, trades);
                rest_id.push_back(r.value_or(0));
                break;
            }
            case EventType::AddMarket:
                book.add_market(e.side, e.qty, trades);
                break;
            case EventType::Cancel:
                if (e.cancel_ordinal < rest_id.size()) {
                    OrderId id = rest_id[e.cancel_ordinal];
                    if (id != 0) book.cancel(id);
                }
                break;
        }
        auto t1 = Clock::now();
        lat.push_back(static_cast<double>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count()));
    }

    auto wall_end = Clock::now();
    double wall_ms = std::chrono::duration<double, std::milli>(
                         wall_end - wall_start).count();

    std::sort(lat.begin(), lat.end());
    auto pct = [&](double p) -> double {
        if (lat.empty()) return 0.0;
        return lat[static_cast<std::size_t>(p / 100.0 * (lat.size() - 1))];
    };

    double throughput = cfg.count / (wall_ms / 1000.0);

    std::ostringstream o;
    o << "{\"events\":"     << event_count
      << ",\"trades\":"     << trades.size()
      << ",\"wall_ms\":"    << wall_ms
      << ",\"throughput\":" << throughput
      << ",\"p50\":"        << pct(50)
      << ",\"p90\":"        << pct(90)
      << ",\"p99\":"        << pct(99)
      << ",\"p999\":"       << pct(99.9)
      << "}";
    return o.str();
}

// ---------------------------------------------------------------------------
// embind registrations — this is what makes JS able to see these symbols
// ---------------------------------------------------------------------------

EMSCRIPTEN_BINDINGS(order_book_module) {
    class_<JSOrderBook>("OrderBook")
        .constructor()
        .function("addLimit",    &JSOrderBook::add_limit)
        .function("addMarket",   &JSOrderBook::add_market)
        .function("cancel",      &JSOrderBook::cancel)
        .function("getSnapshot", &JSOrderBook::get_snapshot)
        .function("reset",       &JSOrderBook::reset);

    function("runSimulation", &run_simulation);
    function("runBenchmark",  &run_benchmark);
}
