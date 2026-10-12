CXX := g++
CXXFLAGS := -std=c++20 -Wall -Wextra -Wpedantic -O2 -Iinclude
TARGET := sysmon

SRCS := $(wildcard src/*.cpp)
OBJS := $(SRCS:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

src/%.o: src/%.cpp $(wildcard include/*.hpp)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f src/*.o $(TARGET) tests/benchmark

test: $(TARGET)
	python3 -m unittest discover -s tests -v

BENCH_ARGS ?= --iterations 100

# Link the production implementations, excluding the interactive main driver.
tests/benchmark: tests/benchmark.cpp $(filter-out src/main.o,$(OBJS)) $(wildcard include/*.hpp)
	$(CXX) $(CXXFLAGS) -o $@ tests/benchmark.cpp $(filter-out src/main.o,$(OBJS))

bench: tests/benchmark
	python3 tests/benchmark.py $(BENCH_ARGS)

.PHONY: all clean test bench
