// OptiCore deterministic random number helpers.
// std::mt19937_64 is standardized (identical stream across platforms and
// compilers), so simulation and solver behaviors are reproducible.
#pragma once

#include <random>

namespace oc {

struct Rng {
  std::mt19937_64 generator;
  explicit Rng(uint64_t seed) : generator(seed) {}
  Rng(const Rng&) = delete;
  Rng& operator=(const Rng&) = delete;

  uint64_t next_u64() { return generator(); }

  double uniform01() {
    std::uniform_real_distribution<double> d(0.0, 1.0);
    return d(generator);
  }

  double uniform(double lo, double hi) {
    std::uniform_real_distribution<double> d(lo, hi);
    return d(generator);
  }

  int uniform_int(int lo, int hi) {  // inclusive
    std::uniform_int_distribution<int> d(lo, hi);
    return d(generator);
  }
};

}  // namespace oc
