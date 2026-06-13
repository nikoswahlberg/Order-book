CXX      ?= g++
CXXFLAGS ?= -std=c++20 -O2 -Wall -Wextra -Wpedantic -Iinclude -Isrc
BUILD    := build

CORE := src/order_book.cpp src/data_gen.cpp

.PHONY: all clean test bench run

all: $(BUILD)/obs $(BUILD)/bench $(BUILD)/tests

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/obs: src/main.cpp $(CORE) | $(BUILD)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD)/bench: bench/benchmark.cpp $(CORE) | $(BUILD)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD)/tests: tests/test_order_book.cpp $(CORE) | $(BUILD)
	$(CXX) $(CXXFLAGS) $^ -o $@

test: $(BUILD)/tests
	./$(BUILD)/tests

bench: $(BUILD)/bench
	./$(BUILD)/bench

run: $(BUILD)/obs
	./$(BUILD)/obs demo

clean:
	rm -rf $(BUILD)
