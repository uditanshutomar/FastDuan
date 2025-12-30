# FastDuan: Adaptive Delta-Stepping SSSP
# Makefile for macOS (Apple Silicon) and Linux

CXX = clang++
CXXFLAGS = -std=c++20 -O3 -march=native -Wall -Wextra

# OpenMP Configuration
BREW_PREFIX := $(shell which brew >/dev/null && brew --prefix libomp 2>/dev/null)

ifeq ($(shell uname), Darwin)
    ifneq ($(BREW_PREFIX),)
        OMP_FLAGS = -Xpreprocessor -fopenmp -I$(BREW_PREFIX)/include
        OMP_LIBS = -L$(BREW_PREFIX)/lib -lomp
    else
        OMP_FLAGS = -fopenmp
        OMP_LIBS = -lomp
    endif
else
    OMP_FLAGS = -fopenmp
    OMP_LIBS = -lgomp
endif

INCLUDES = -Iinclude

.PHONY: all clean test

all: adaptive_sssp parallel_sssp geometric_sssp delta_heuristics test_suite

adaptive_sssp: src/adaptive_sssp.cpp include/fast_duan/*.hpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< -o $@

parallel_sssp: src/parallel_sssp.cpp include/fast_duan/*.hpp
	$(CXX) $(CXXFLAGS) $(OMP_FLAGS) $(INCLUDES) $< -o $@ $(OMP_LIBS)

geometric_sssp: src/geometric_sssp.cpp include/fast_duan/*.hpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< -o $@

delta_heuristics: bench/delta_heuristics.cpp include/fast_duan/*.hpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< -o $@

test_suite: tests/verify_correctness.cpp include/fast_duan/*.hpp
	$(CXX) $(CXXFLAGS) $(OMP_FLAGS) $(INCLUDES) $< -o $@ $(OMP_LIBS)

test: test_suite
	./test_suite

clean:
	rm -f adaptive_sssp parallel_sssp geometric_sssp delta_heuristics test_suite
