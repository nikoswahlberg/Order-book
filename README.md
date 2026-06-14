# Limit Order Book Simulator

A from-scratch limit order book and matching engine in modern C++ (C++20),
modelled on a power-exchange order book (POWER/EUR, prices in EUR/MWh). It
implements **price-time priority** matching, limit and market orders, O(1)
cancellation, synthetic order-flow generation, and a benchmark harness that
reports throughput and latency percentiles.

On an Apple M3 Pro (single core, `-O2`) it sustains **10,4 million events per
second** with a **median matching latency of 83 ns**.

The matching engine is also compiled to **WebAssembly** and runs entirely in
the browser — no server, no backend. Try the live demo in `web/`.

## Why an order book?

Every exchange — equities, futures, and power markets such as Nord Pool,
Nasdaq Commodities, and the intraday platforms used by balance-responsible
parties — matches buyers and sellers through an order book. It holds all
outstanding buy orders (bids) and sell orders (asks) sorted by price, and a
matching engine decides who trades with whom. The two rules are:

1. **Price priority** — the best price is served first (highest bid, lowest ask).
2. **Time priority** — among orders at the same price, the one that arrived first is filled first.

A trade happens whenever the best bid price meets or exceeds the best ask
price. The execution price is the resting order's price (the "maker"); the
incoming order that crosses the spread is the "taker".

## Features

- Limit orders (rest in the book, partially fill, or cross immediately)
- Market orders (sweep the opposite side until filled or the book runs dry)
- O(1) average cancellation by order id
- Integer tick prices (no floating-point comparison in the hot path)
- Aggregated L2 depth snapshots for display / market data
- Synthetic order-flow generator with configurable mix of adds, cancels, market orders, and aggressive (crossing) orders
- Benchmark harness: throughput plus p50 / p90 / p99 / p99,9 / max latency
- WebAssembly build: the full C++ engine compiled to WASM and running in the browser
- Interactive web demo: live depth chart, trade feed, simulation scenarios, in-browser benchmark
- Unit tests with no external dependencies

## Build

Requires a C++20 compiler. Two options:

```bash
# Option A — Make
make all          # builds build/obs, build/bench, build/tests
make test         # build and run the tests
make run          # build and run the demo

# Option B — CMake
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
```

## Usage

### Scripted demo

```bash
./build/obs demo
```

Seeds a book around 50,00 EUR/MWh, then walks through a crossing limit order
and a market order that sweeps several price levels, printing the book at each step:

```
A seller sends a MARKET SELL 25 MW (sweeps the bids)
   TRADE 3 MW @ 50.20  (maker #7, taker #8)
   TRADE 10 MW @ 49.80 (maker #1, taker #8)
   TRADE 12 MW @ 49.50 (maker #2, taker #8)
```

### Live simulation

```bash
./build/obs sim 100000      # replay 100 000 generated events in 5 snapshots
```

### Benchmark

```bash
./build/bench 5000000       # replay 5 000 000 events, report timings
```

## WebAssembly demo

The matching engine compiles to WASM via Emscripten. The browser runs the
actual C++ code — no server involved.

### Build the WASM module (one-time)

```bash
brew install emscripten     # Mac, one-time
./web/build_wasm.sh         # produces web/order_book.js + web/order_book.wasm
```

### Run locally

```bash
cd web && python3 -m http.server 8080
open http://localhost:8080
```

### What the demo includes

- Live L2 order book table with depth bars updating in real time
- Market depth chart (bid/ask sides visualised with Chart.js)
- Auto-simulation mode firing random orders continuously
- Manual order placement: limit, market, cancel
- Simulation panel with preset scenarios (Normal, Volatile, Thin book, HFT-like) and parameter sliders
- In-browser benchmark reporting throughput and latency percentiles from your own CPU

## Design

### Data structures

Each side of the book is a sorted map from price to a FIFO queue of orders:

```
bids:  std::map<Price, std::list<Order>, std::greater<Price>>   // best = begin()
asks:  std::map<Price, std::list<Order>, std::less<Price>>      // best = begin()
index: std::unordered_map<OrderId, Location>                    // for O(1) cancel
```

Three deliberate choices drive the performance and correctness:

- **`std::map` per side.** The tree keeps price levels sorted, so the best bid
  or ask is always `begin()` — O(1) to read, O(log L) to insert a new level
  (L = number of distinct price levels, typically small).
- **`std::list` per price level.** A linked list gives FIFO time priority and,
  crucially, *stable iterators*: removing one order never invalidates the
  iterators pointing at the others. That is what makes the cancel index work.
- **Order index.** A hash map from order id to its exact location (side, price,
  and a `std::list` iterator) lets `cancel()` jump straight to the order
  instead of scanning the book.

### Integer prices

Prices are stored as `int64_t` ticks (1 tick = 0,01 EUR), never as `double`.
Floating-point equality is unreliable — 49,80 might be stored as 49,799999 —
and integer comparison is both exact and faster. Conversion happens only at the
display boundary.

### Complexity

| Operation                  | Cost                                         |
|----------------------------|----------------------------------------------|
| best bid / ask / spread    | O(1)                                         |
| add limit, no cross        | O(log L) to find/create the price level      |
| each fill while matching   | O(1), plus O(log L) when a level empties     |
| cancel                     | O(1) average, plus O(log L) if level empties |
| L2 depth snapshot (N deep) | O(N + orders in those N levels)              |

(L = number of distinct price levels resting in the book.)

## Benchmark results

Measured on Apple M3 Pro, single core, `-O2`, 5 000 000 generated events.

| Metric          | Value                 |
|-----------------|-----------------------|
| events          | 5 000 000             |
| trades produced | ~3 340 000            |
| throughput      | 10,4 million events/s |
| latency p50     | 83 ns                 |
| latency p90     | 166 ns                |
| latency p99     | 334 ns                |
| latency p99,9   | 1 084 ns              |

Two honest caveats: the per-event latency includes the cost of two clock reads
(roughly 50–80 ns of measurement overhead), and the maximum latency is
dominated by rare OS scheduling pauses rather than the algorithm itself. The
p50/p99 figures are the meaningful ones.

## Project layout

```
include/obs/        public headers
  price.hpp           tick type and conversions
  order.hpp           Order and Trade structs
  order_book.hpp      OrderBook interface
src/
  order_book.cpp      matching, resting, cancellation
  data_gen.{hpp,cpp}  synthetic order-flow generator
  driver.hpp          replays an event stream into a book
  main.cpp            CLI: demo, sim
bench/
  benchmark.cpp       throughput and latency harness
tests/
  test_order_book.cpp assertion-based unit tests
web/
  bindings.cpp        Emscripten/embind glue exposing the engine to JavaScript
  build_wasm.sh       one-command WASM build script
  index.html          self-contained interactive demo (Chart.js, no framework)
  order_book.js       generated — produced by build_wasm.sh
  order_book.wasm     generated — produced by build_wasm.sh
```

## Possible extensions

- A Phase-2 **Elspot-style bidding layer** on top of the engine, with SRMC-based bid curves.
- Replace the `std::map` levels with a **flat array of price levels** (a common production technique when the price range is bounded) and benchmark the difference.
- Record an **L2 market-data feed** to file and replay it.
- Add **self-trade prevention** and **iceberg / hidden** order types.
- Host the WASM demo on **GitHub Pages**.

## License

MIT — see `LICENSE`.