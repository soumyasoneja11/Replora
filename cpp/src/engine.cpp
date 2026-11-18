#include "replora/engine.hpp"

#include "replora/fifo_policy.hpp"
#include "replora/lfu_policy.hpp"
#include "replora/lru_policy.hpp"
#include "replora/random_policy.hpp"
#include "replora/workloads.hpp"

#include <chrono>
#include <memory>
#include <utility>

namespace replora {
namespace {

struct RecordedStep {
  AccessResult result;
  CacheState state;
};

const char* kPolicyNames[] = {"FIFO", "LRU", "LFU", "RANDOM"};

bool knownPolicy(const std::string& name) {
  for (const char* policy : kPolicyNames) {
    if (name == policy) {
      return true;
    }
  }
  return false;
}

std::unique_ptr<CachePolicy> makePolicy(const std::string& name, std::size_t capacity,
                                        std::uint32_t seed) {
  if (name == "FIFO") {
    return std::make_unique<FifoPolicy>(capacity);
  }
  if (name == "LRU") {
    return std::make_unique<LruPolicy>(capacity);
  }
  if (name == "LFU") {
    return std::make_unique<LfuPolicy>(capacity);
  }
  return std::make_unique<RandomPolicy>(capacity, seed);
}

PolicyResult runPolicy(const std::string& name, std::size_t capacity, std::uint32_t seed,
                       const std::vector<Key>& trace) {
  const std::unique_ptr<CachePolicy> policy = makePolicy(name, capacity, seed);
  std::vector<RecordedStep> steps;
  steps.reserve(trace.size());

  // Clock covers access() and the state snapshot only. Explanation strings are later.
  const auto started = std::chrono::steady_clock::now();
  for (const Key key : trace) {
    RecordedStep step;
    step.result = policy->access(key);
    step.state = policy->getCacheState();
    steps.push_back(std::move(step));
  }
  const auto finished = std::chrono::steady_clock::now();
  const auto micros =
      std::chrono::duration_cast<std::chrono::microseconds>(finished - started).count();

  const std::uint64_t memoryBytes = policy->estimateMemoryBytes();

  PolicyResult outcome;
  outcome.policyName = name;
  outcome.events.reserve(steps.size());
  for (std::size_t i = 0; i < steps.size(); ++i) {
    AccessEvent event;
    event.index = static_cast<std::uint64_t>(i);
    event.key = trace[i];
    event.hit = steps[i].result.hit;
    event.evicted = steps[i].result.evicted;
    event.victim = steps[i].result.victim;
    event.stateAfter = std::move(steps[i].state);
    event.policyName = name;
    event.explanation = explainAccess(name, trace[i], steps[i].result, event.stateAfter);
    outcome.events.push_back(std::move(event));
  }

  std::uint64_t hits = 0;
  std::uint64_t misses = 0;
  std::uint64_t evictions = 0;
  for (const AccessEvent& event : outcome.events) {
    if (event.hit) {
      ++hits;
    } else {
      ++misses;
    }
    if (event.evicted) {
      ++evictions;
    }
  }

  const std::uint64_t total = outcome.events.size();
  outcome.metrics.hits = hits;
  outcome.metrics.misses = misses;
  outcome.metrics.evictions = evictions;
  outcome.metrics.hitRate = total == 0 ? 0.0 : (static_cast<double>(hits) * 100.0) / static_cast<double>(total);
  outcome.metrics.missRate = total == 0 ? 0.0 : (static_cast<double>(misses) * 100.0) / static_cast<double>(total);
  outcome.metrics.executionTimeMicros = static_cast<std::uint64_t>(micros < 0 ? 0 : micros);
  outcome.metrics.executionTimeMs = static_cast<double>(outcome.metrics.executionTimeMicros) / 1000.0;
  outcome.metrics.totalAccesses = total;
  outcome.metrics.cacheCapacity = capacity;
  outcome.metrics.estimatedMemoryBytes = memoryBytes;
  return outcome;
}

}  // namespace

SimulationResult runSimulation(const SimulationConfig& config) {
  SimulationResult result;
  result.config = config;

  if (config.capacity < 1) {
    result.errors.push_back(ConfigError{"capacity", "capacity must be >= 1"});
  }
  if (config.traceLength < 0) {
    result.errors.push_back(ConfigError{"traceLength", "traceLength must be >= 0"});
  } else if (config.traceLength > kMaxTraceLength) {
    result.errors.push_back(
        ConfigError{"traceLength", "traceLength must be <= 10000"});
  }

  if (!config.workloadName.empty()) {
    const std::optional<WorkloadType> parsed = workloadFromString(config.workloadName);
    if (!parsed.has_value()) {
      result.errors.push_back(ConfigError{"workload", "unknown workload"});
    } else {
      result.config.workload = *parsed;
    }
  }

  if (config.policySelection == PolicySelection::One && config.policyName.empty()) {
    result.errors.push_back(ConfigError{"policy", "unknown policy"});
  } else if (!config.policyName.empty() && !knownPolicy(config.policyName)) {
    result.errors.push_back(ConfigError{"policy", "unknown policy"});
  }

  if (!result.errors.empty()) {
    return result;
  }

  const std::vector<Key> trace =
      generateTrace(result.config.workload, static_cast<std::size_t>(result.config.traceLength),
                    result.config.seed, static_cast<std::size_t>(result.config.capacity));
  result.trace = trace;

  const std::size_t capacity = static_cast<std::size_t>(result.config.capacity);
  if (result.config.policySelection == PolicySelection::One) {
    result.results.push_back(runPolicy(result.config.policyName, capacity, result.config.seed, trace));
    return result;
  }

  for (const char* name : kPolicyNames) {
    result.results.push_back(runPolicy(name, capacity, result.config.seed, trace));
  }
  return result;
}

}  // namespace replora
