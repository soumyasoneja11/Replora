#pragma once

#include "replora/types.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace replora {

// Trace generators ported from src/simulation/workloads.ts.
// Same formulas, same Rng draw order, same loop size, burst shape, and skew.
// capacity is accepted so callers can pass the cache size. The TypeScript
// generator does not use it, and neither does this one.
//
// sequential: keys 1, 2, 3, ... with no repeats. Models a scan of new blocks.
//   Every policy misses; the trace shows compulsory misses and no reuse.
// looping: 1, 2, 3, 4 repeating. Models a tiny working set.
//   A cache of at least 4 hits after warmup. A smaller cache thrashes in a cycle.
// random: uniform keys in 1..20. Models unstructured traffic.
//   Hit rate depends on capacity versus that 20-key range, not on recency or frequency.
// bursty: runs of one hot key (length 3..10) with a 20% chance of a noise key in 11..20.
//   Models short temporal locality. Recency policies can keep the burst key.
// frequency-skewed: 70% from hot keys {1, 2, 3}, otherwise a cold key in 4..15.
//   Models a stable popularity skew. Frequency counting can protect the hot keys.

std::vector<Key> generateTrace(WorkloadType workload, std::size_t length, std::uint32_t seed = 42,
                               std::optional<std::size_t> capacity = std::nullopt);

}  // namespace replora
