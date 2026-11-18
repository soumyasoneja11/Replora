#include "replora/workloads.hpp"

#include "replora/rng.hpp"

#include <algorithm>

namespace replora {
namespace {

std::vector<Key> generateSequential(std::size_t length) {
  std::vector<Key> trace;
  trace.reserve(length);
  for (std::size_t i = 0; i < length; ++i) {
    trace.push_back(static_cast<Key>(i + 1));
  }
  return trace;
}

std::vector<Key> generateLooping(std::size_t length) {
  constexpr Key kLoop[] = {1, 2, 3, 4};
  std::vector<Key> trace;
  trace.reserve(length);
  for (std::size_t i = 0; i < length; ++i) {
    trace.push_back(kLoop[i % 4]);
  }
  return trace;
}

std::vector<Key> generateRandom(std::size_t length, std::uint32_t seed) {
  Rng rng(seed);
  constexpr std::uint32_t kRange = 20;
  std::vector<Key> trace;
  trace.reserve(length);
  for (std::size_t i = 0; i < length; ++i) {
    trace.push_back(static_cast<Key>(rng.nextBelow(kRange) + 1));
  }
  return trace;
}

std::vector<Key> generateBursty(std::size_t length, std::uint32_t seed) {
  Rng rng(seed);
  std::vector<Key> trace;
  trace.reserve(length);
  std::size_t i = 0;
  while (i < length) {
    const Key burstItem = static_cast<Key>(rng.nextBelow(10) + 1);
    const std::size_t burstLen = std::min<std::size_t>(3 + rng.nextBelow(8), length - i);
    for (std::size_t j = 0; j < burstLen; ++j) {
      if (rng.nextUnit() < 0.2) {
        trace.push_back(static_cast<Key>(rng.nextBelow(10) + 11));
      } else {
        trace.push_back(burstItem);
      }
      ++i;
    }
  }
  return trace;
}

std::vector<Key> generateFrequencySkewed(std::size_t length, std::uint32_t seed) {
  Rng rng(seed);
  constexpr Key kHot[] = {1, 2, 3};
  constexpr Key kCold[] = {4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
  std::vector<Key> trace;
  trace.reserve(length);
  for (std::size_t i = 0; i < length; ++i) {
    if (rng.nextUnit() < 0.7) {
      trace.push_back(kHot[rng.nextBelow(3)]);
    } else {
      trace.push_back(kCold[rng.nextBelow(12)]);
    }
  }
  return trace;
}

}  // namespace

std::vector<Key> generateTrace(WorkloadType workload, std::size_t length, std::uint32_t seed,
                               std::optional<std::size_t> /*capacity*/) {
  switch (workload) {
    case WorkloadType::Sequential:
      return generateSequential(length);
    case WorkloadType::Random:
      return generateRandom(length, seed);
    case WorkloadType::Looping:
      return generateLooping(length);
    case WorkloadType::Bursty:
      return generateBursty(length, seed);
    case WorkloadType::FrequencySkewed:
      return generateFrequencySkewed(length, seed);
  }
  return {};
}

}  // namespace replora
