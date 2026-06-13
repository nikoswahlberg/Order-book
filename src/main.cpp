#include "obs/order_book.hpp"
#include "data_gen.hpp"
#include "driver.hpp"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace obs;

namespace {

void print_book(const OrderBook& book, std::size_t levels = 6) {
    auto asks = book.ask_depth(levels);
    auto bids = book.bid_depth(levels);

    std::printf("\n        ORDER BOOK  (POWER/EUR, EUR/MWh)\n");
    std::printf("   %-8s %-10s %-8s\n", "Orders", "Qty (MW)", "Price");
    std::printf("   ---------------------------------\n");
    // Asks printed high-to-low so best ask sits just above the spread.
    for (auto it = asks.rbegin(); it != asks.rend(); ++it) {
        std::printf("   %-8zu %-10lld \033[31m%8.2f\033[0m  ask\n",
                    it->orders, static_cast<long long>(it->quantity),
                    to_price(it->price));
    }
    auto sp = book.spread();
    if (sp) {
        std::printf("   --------- spread: %.2f -----------\n", to_price(*sp));
    } else {
        std::printf("   ------------ spread: -- ----------\n");
    }
    for (const auto& lv : bids) {
        std::printf("   %-8zu %-10lld \033[32m%8.2f\033[0m  bid\n",
                    lv.orders, static_cast<long long>(lv.quantity),
                    to_price(lv.price));
    }
    std::printf("\n");
}

int run_demo() {
    OrderBook book;
    std::vector<Trade> trades;

    std::printf("Seeding a book around 50.00 EUR/MWh...\n");
    book.add_limit(Side::Buy, to_ticks(49.80), 10, trades);
    book.add_limit(Side::Buy, to_ticks(49.50), 20, trades);
    book.add_limit(Side::Buy, to_ticks(49.20), 8, trades);
    book.add_limit(Side::Sell, to_ticks(50.20), 12, trades);
    book.add_limit(Side::Sell, to_ticks(50.50), 7, trades);
    book.add_limit(Side::Sell, to_ticks(50.80), 18, trades);
    print_book(book);

    std::printf("A buyer sends a limit BUY 15 MW @ 50.20 (crosses best ask)\n");
    trades.clear();
    book.add_limit(Side::Buy, to_ticks(50.20), 15, trades);
    for (const Trade& t : trades) {
        std::printf("   \033[33mTRADE\033[0m %lld MW @ %.2f  (maker #%llu, taker #%llu)\n",
                    static_cast<long long>(t.quantity), to_price(t.price),
                    static_cast<unsigned long long>(t.maker_id),
                    static_cast<unsigned long long>(t.taker_id));
    }
    std::printf("  -> 12 MW filled at 50.20; the remaining 3 MW rests as a new bid.\n");
    print_book(book);

    std::printf("A seller sends a MARKET SELL 25 MW (sweeps the bids)\n");
    trades.clear();
    Quantity unfilled = book.add_market(Side::Sell, 25, trades);
    for (const Trade& t : trades) {
        std::printf("   \033[33mTRADE\033[0m %lld MW @ %.2f  (maker #%llu, taker #%llu)\n",
                    static_cast<long long>(t.quantity), to_price(t.price),
                    static_cast<unsigned long long>(t.maker_id),
                    static_cast<unsigned long long>(t.taker_id));
    }
    if (unfilled > 0)
        std::printf("  -> %lld MW could not be filled (book ran dry).\n",
                    static_cast<long long>(unfilled));
    print_book(book);
    return 0;
}

int run_sim(std::uint64_t events) {
    GenConfig cfg;
    cfg.count = events;
    auto stream = generate_events(cfg);

    OrderBook book;
    std::vector<Trade> trades;
    trades.reserve(stream.size() / 2);

    std::printf("Replaying %llu events...\n",
                static_cast<unsigned long long>(events));

    // Replay in chunks so we can show the book evolving.
    Driver driver(book);
    std::uint64_t done = 0;
    const std::uint64_t chunk = events / 5 > 0 ? events / 5 : events;
    for (int step = 0; step < 5 && done < events; ++step) {
        std::uint64_t end = std::min(done + chunk, events);
        std::vector<Event> slice(stream.begin() + done, stream.begin() + end);
        driver.replay(slice, trades);
        done = end;
        std::printf("--- after %llu events: %zu resting orders, %zu trades ---\n",
                    static_cast<unsigned long long>(done),
                    book.resting_orders(), trades.size());
        print_book(book, 5);
    }
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::printf("usage:\n  obs demo\n  obs sim [events]\n  obs bench [events]   (separate binary: ./bench)\n");
        return 1;
    }
    if (std::strcmp(argv[1], "demo") == 0) {
        return run_demo();
    }
    if (std::strcmp(argv[1], "sim") == 0) {
        std::uint64_t n = argc >= 3 ? std::stoull(argv[2]) : 200;
        return run_sim(n);
    }
    std::printf("unknown command: %s\n", argv[1]);
    return 1;
}
