#pragma once

#include "replora/types.hpp"

#include <cstdint>
#include <string>

namespace replora {

// Traces longer than this are a ConfigError. Length 0 is valid.
constexpr std::int64_t kMaxTraceLength = 10000;

// One shared trace, then each selected policy on a fresh instance.
// Validation failures come back in SimulationResult::errors and do not throw.
SimulationResult runSimulation(const SimulationConfig& config);

// Second-pass text. Not called from the timed access loop.
std::string explainAccess(const std::string& policyName, Key key, const AccessResult& result,
                          const CacheState& state);

}  // namespace replora
