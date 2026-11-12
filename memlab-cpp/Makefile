CXX      ?= g++
WARN     := -Wall -Wextra -Wpedantic
CXXFLAGS := -std=c++17 $(WARN) -O2

SRC := allocator.cpp

all: memlab

memlab: main.cpp $(SRC) allocator.hpp
	$(CXX) $(CXXFLAGS) main.cpp $(SRC) -o $@

# Tests are built with AddressSanitizer + UBSan to catch memory bugs in the allocator itself.
tests: tests.cpp $(SRC) allocator.hpp
	$(CXX) -std=c++17 $(WARN) -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer tests.cpp $(SRC) -o $@

test: tests
	./tests

demo: memlab
	./memlab demo

bench: memlab
	./memlab bench

clean:
	rm -f memlab tests

.PHONY: all test demo bench clean
