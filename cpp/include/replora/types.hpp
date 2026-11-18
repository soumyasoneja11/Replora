#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace replora {

// Trace keys in src/simulation/types.ts are TypeScript `number`
// (requestedItem, victim, cacheState). Generators emit small integers.
// int32_t is that integer domain in C++.
using Key = std::int32_t;

// Wire strings match the TypeScript WorkloadType union:
// sequential, random, looping, bursty, frequency-skewed.
enum class WorkloadType : std::uint8_t {
  Sequential = 0,
  Random = 1,
  Looping = 2,
  Bursty = 3,
  FrequencySkewed = 4,
};

const char* toString(WorkloadType workload);
std::optional<WorkloadType> workloadFromString(std::string_view text);

// Stable numeric values so a later JSON mapping does not depend on declaration order.
enum class ReasonCode : std::uint8_t {
  Hit = 0,
  MissInsertedFree = 1,
  MissEvictedFifoOldest = 2,
  MissEvictedLruTail = 3,
  MissEvictedLfuMinFreq = 4,
  MissEvictedRandomIndex = 5,
};

// Lightweight access outcome. No explanation string: formatting happens
// outside the timed region.
struct AccessResult {
  bool hit = false;
  bool evicted = false;
  std::optional<Key> victim;
  ReasonCode reason = ReasonCode::Hit;
};

// Resident keys in structure order, plus a label such as "front=MRU".
// frequencies is parallel to keys. Empty when the policy has no frequency counts.
struct CacheState {
  std::vector<Key> keys;
  std::vector<std::uint64_t> frequencies;
  std::string ordering;
};

struct AccessEvent {
  std::uint64_t index = 0;
  Key key = 0;
  bool hit = false;
  bool evicted = false;
  std::optional<Key> victim;
  CacheState stateAfter;
  std::string policyName;
  std::string explanation;
};

struct PolicyMetrics {
  std::uint64_t hits = 0;
  std::uint64_t misses = 0;
  std::uint64_t evictions = 0;
  double hitRate = 0.0;
  double missRate = 0.0;
  double executionTimeMs = 0.0;
  // steady_clock around the access loop only. executionTimeMs is this value / 1000.
  std::uint64_t executionTimeMicros = 0;
  std::uint64_t totalAccesses = 0;
  std::uint64_t cacheCapacity = 0;
  // Container-size estimate. Never process RSS. Zero until a policy fills it.
  std::uint64_t estimatedMemoryBytes = 0;
};

// Per-policy slice of a simulation. Value types only, for later JSON.
struct PolicyResult {
  std::string policyName;
  PolicyMetrics metrics;
  std::vector<AccessEvent> events;
};

enum class PolicySelection : std::uint8_t { All = 0, One = 1 };

// Defaults match the lab: looping, capacity 4, length 100, seed 42, all policies.
// capacity and traceLength are signed so a negative value is a ConfigError.
// If workloadName is non-empty it is parsed and replaces workload.
// policyName is required when policySelection is One.
struct SimulationConfig {
  WorkloadType workload = WorkloadType::Looping;
  std::int64_t capacity = 4;
  std::int64_t traceLength = 100;
  std::uint32_t seed = 42;
  PolicySelection policySelection = PolicySelection::All;
  std::string policyName;
  std::string workloadName;
};

// Structured validation failure (for example capacity 0). No exceptions in the result.
struct ConfigError {
  std::string field;
  std::string message;
};

struct SimulationResult {
  SimulationConfig config;
  std::vector<Key> trace;
  std::vector<PolicyResult> results;
  std::vector<ConfigError> errors;
};

}  // namespace replora
