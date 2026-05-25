CXX ?= c++
CXXFLAGS ?= -std=c++20 -O2 -Wall -Wextra -Wpedantic -Iinclude

CORE_SRCS = \
	src/benchmark.cpp \
	src/feature_export.cpp \
	src/ml_model.cpp \
	src/schedulers/fifo_scheduler.cpp \
	src/schedulers/ml_scheduler.cpp \
	src/schedulers/mlfq_scheduler.cpp \
	src/simulator.cpp \
	src/trace_loader.cpp \
	src/types.cpp \
	src/workload.cpp

.PHONY: all test clean

all: build/scheduler_sim build/scheduler_tests

build:
	mkdir -p build

build/scheduler_sim: build $(CORE_SRCS) src/main.cpp
	$(CXX) $(CXXFLAGS) $(CORE_SRCS) src/main.cpp -o $@

build/scheduler_tests: build $(CORE_SRCS) tests/test_scheduler.cpp
	$(CXX) $(CXXFLAGS) $(CORE_SRCS) tests/test_scheduler.cpp -o $@

test: build/scheduler_tests
	./build/scheduler_tests

clean:
	rm -rf build
