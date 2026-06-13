#include "obs/order_book.hpp"
#include "data_gen.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using namespace obs;
using Clock = std::chrono::steady_clock;

namespace {

// --- European number formatting (space thousands, comma decimal) ----------
std::string group(std::uint64_t v) {
    std::string s = std::to_string(v);
    std::string out;
    int c = 0;
    for (auto it = s.rbegin(); it != s.rend(); ++it) {
        if (c && c % 3 == 0) out.push_back(' ');
        out.push_back(*it);
        ++c;
    }
    std::reverse(out.begin(), out.end());
    return out;
}

std::string dec(double v, int places = 2) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.*f", places, v);
    std::string s(buf);
    for (char& ch : s)
        if (ch == '.') ch = ',';
    return s;
}

double pct(std::vector<std::uint32_t>& lat, double p) {
    if (lat.empty()) return 0.0;
    std::size_t idx = static_cast<std::size_t>(p / 100.0 * (lat.size() - 1));
    return static_cast<double>(lat[idx]);
}

} // namespace

int main(int argc, char** argv) {
    GenConfig cfg;
    cfg.count = argc >= 2 ? std::stoull(argv[1]) : 5'000'000;

    std::printf("Generating %s events (seed %llu)...\n",
                group(cfg.count).c_str(),
                static_cast<unsigned long long>(cfg.seed));
    auto events = generate_events(cfg);

    OrderBook book;
    std::vector<Trade> trades;
    trades.reserve(events.size()); // pre-size so matching never reallocates

    std::vector<std::uint32_t> lat;  // per-event latency in nanoseconds
    lat.reserve(events.size());
    std::vector<OrderId> rest_id;
    rest_id.reserve(events.size());

    std::printf("Replaying through the matching engine...\n");
    auto wall_start = Clock::now();

    for (const Event& e : events) {
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
        lat.push_back(static_cast<std::uint32_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0)
                .count()));
    }

    auto wall_end = Clock::now();
    double secs =
        std::chrono::duration<double>(wall_end - wall_start).count();

    std::sort(lat.begin(), lat.end());
    double mean = 0;
    for (auto v : lat) mean += v;
    mean /= lat.size();

    const double throughput = cfg.count / secs;

    std::printf("\n================ RESULTS ================\n");
    std::printf("events            %s\n", group(cfg.count).c_str());
    std::printf("trades produced   %s\n", group(trades.size()).c_str());
    std::printf("resting orders    %s\n", group(book.resting_orders()).c_str());
    std::printf("wall time         %s s\n", dec(secs, 3).c_str());
    std::printf("throughput        %s events/s\n",
                group(static_cast<std::uint64_t>(throughput)).c_str());
    std::printf("-----------------------------------------\n");
    std::printf("latency per event (nanoseconds)\n");
    std::printf("  mean            %s ns\n", dec(mean, 1).c_str());
    std::printf("  p50             %s ns\n", dec(pct(lat, 50), 0).c_str());
    std::printf("  p90             %s ns\n", dec(pct(lat, 90), 0).c_str());
    std::printf("  p99             %s ns\n", dec(pct(lat, 99), 0).c_str());
    std::printf("  p99.9           %s ns\n", dec(pct(lat, 99.9), 0).c_str());
    std::printf("  max             %s ns\n",
                group(lat.empty() ? 0 : lat.back()).c_str());
    std::printf("=========================================\n");
    return 0;
}
