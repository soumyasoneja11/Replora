#pragma once

#include <cstdint>

namespace replora {

// Same LCG as src/simulation/workloads.ts and src/algorithms/random.ts.
// state starts at the raw seed. The first draw advances first.
//   state = (state * 1664525 + 1013904223) mod 2^32
//   unit = state / 4294967296.0   in [0, 1)
// Integer draws use floor(unit * range), not state % range.
class Rng {
 public:
  explicit Rng(std::uint32_t seed);

  double nextUnit();
  std::uint32_t nextBelow(std::uint32_t exclusiveUpper);
  void reset();

 private:
  std::uint32_t seed_;
  std::uint32_t state_;
};

}  // namespace replora
