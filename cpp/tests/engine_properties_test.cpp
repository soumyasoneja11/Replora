#include "replora/engine.hpp"
#include "replora/workloads.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <set>
#include <string>
#include <vector>

namespace {

bool contains(const std::vector<replora::Key>& keys, replora::Key key) {
  for (const replora::Key resident : keys) {
    if (resident == key) {
      return true;
    }
  }
  return false;
}

bool hasDuplicate(const std::vector<replora::Key>& keys) {
  std::set<replora::Key> seen;
  for (const replora::Key key : keys) {
    if (!seen.insert(key).second) {
      return true;
    }
  }
  return false;
}

std::string requiredStructure(const std::string& policyName) {
  if (policyName == "FIFO") {
    return "unordered_set";
  }
  if (policyName == "LRU") {
    return "std::list";
  }
  if (policyName == "LFU") {
    return "unordered_map";
  }
  return "vector";
}

void expectInvariants(const replora::SimulationResult& result) {
  ASSERT_TRUE(result.errors.empty());
  ASSERT_GE(result.results.size(), 1u);
  const std::vector<replora::Key>& trace = result.trace;

  for (const replora::PolicyResult& policy : result.results) {
    ASSERT_EQ(policy.events.size(), trace.size()) << policy.policyName;
    std::vector<replora::Key> previous;
    for (std::size_t i = 0; i < policy.events.size(); ++i) {
      const replora::AccessEvent& event = policy.events[i];
      EXPECT_EQ(event.key, trace[i]) << policy.policyName << " step " << i;
      EXPECT_LE(event.stateAfter.keys.size(), static_cast<std::size_t>(result.config.capacity));
      EXPECT_FALSE(hasDuplicate(event.stateAfter.keys)) << policy.policyName << " step " << i;
      EXPECT_FALSE(event.explanation.empty());
      EXPECT_NE(event.explanation.find(requiredStructure(policy.policyName)), std::string::npos)
          << policy.policyName << " " << event.explanation;

      if (event.hit) {
        EXPECT_TRUE(contains(previous, event.key)) << policy.policyName << " step " << i;
      } else {
        EXPECT_TRUE(contains(event.stateAfter.keys, event.key)) << policy.policyName << " step " << i;
      }
      if (event.evicted) {
        ASSERT_TRUE(event.victim.has_value()) << policy.policyName << " step " << i;
        EXPECT_TRUE(contains(previous, *event.victim));
        EXPECT_FALSE(contains(event.stateAfter.keys, *event.victim));
        EXPECT_EQ(previous.size(), static_cast<std::size_t>(result.config.capacity));
      }
      previous = event.stateAfter.keys;
    }
  }

  for (std::size_t i = 1; i < result.results.size(); ++i) {
    ASSERT_EQ(result.results[i].events.size(), result.results[0].events.size());
    for (std::size_t step = 0; step < result.results[0].events.size(); ++step) {
      EXPECT_EQ(result.results[i].events[step].key, result.results[0].events[step].key);
    }
  }
}

bool sameEvent(const replora::AccessEvent& left, const replora::AccessEvent& right) {
  return left.index == right.index && left.key == right.key && left.hit == right.hit &&
         left.evicted == right.evicted && left.victim == right.victim &&
         left.stateAfter.keys == right.stateAfter.keys &&
         left.stateAfter.frequencies == right.stateAfter.frequencies &&
         left.stateAfter.ordering == right.stateAfter.ordering && left.policyName == right.policyName &&
         left.explanation == right.explanation;
}

bool sameMetricsExceptTime(const replora::PolicyMetrics& left, const replora::PolicyMetrics& right) {
  return left.hits == right.hits && left.misses == right.misses && left.evictions == right.evictions &&
         left.hitRate == right.hitRate && left.missRate == right.missRate &&
         left.totalAccesses == right.totalAccesses && left.cacheCapacity == right.cacheCapacity &&
         left.estimatedMemoryBytes == right.estimatedMemoryBytes;
}

TEST(EngineProperties, InvariantsAcrossWorkloadsCapacitiesAndSeeds) {
  const replora::WorkloadType workloads[] = {
      replora::WorkloadType::Sequential, replora::WorkloadType::Random,
      replora::WorkloadType::Looping,    replora::WorkloadType::Bursty,
      replora::WorkloadType::FrequencySkewed,
  };
  const std::int64_t capacities[] = {1, 2, 3, 4, 8, 16};
  const std::uint32_t seeds[] = {1, 42, 99};

  for (const replora::WorkloadType workload : workloads) {
    for (const std::int64_t capacity : capacities) {
      for (const std::uint32_t seed : seeds) {
        replora::SimulationConfig config;
        config.workload = workload;
        config.capacity = capacity;
        config.traceLength = 48;
        config.seed = seed;
        const replora::SimulationResult result = replora::runSimulation(config);
        expectInvariants(result);

        std::set<replora::Key> distinct(result.trace.begin(), result.trace.end());
        if (static_cast<std::size_t>(capacity) < distinct.size()) {
          continue;
        }
        for (const replora::PolicyResult& policy : result.results) {
          EXPECT_EQ(policy.metrics.evictions, 0u)
              << replora::toString(workload) << " cap " << capacity << " " << policy.policyName;
          EXPECT_EQ(policy.metrics.hits,
                    static_cast<std::uint64_t>(config.traceLength) - distinct.size())
              << replora::toString(workload) << " cap " << capacity << " " << policy.policyName;
        }
      }
    }
  }
}

TEST(EngineProperties, SameConfigIsDeterministicExceptTiming) {
  replora::SimulationConfig config;
  config.workload = replora::WorkloadType::Bursty;
  config.capacity = 4;
  config.traceLength = 80;
  config.seed = 99;

  const replora::SimulationResult first = replora::runSimulation(config);
  const replora::SimulationResult second = replora::runSimulation(config);
  ASSERT_TRUE(first.errors.empty());
  ASSERT_EQ(first.trace, second.trace);
  ASSERT_EQ(first.results.size(), second.results.size());
  for (std::size_t p = 0; p < first.results.size(); ++p) {
    EXPECT_EQ(first.results[p].policyName, second.results[p].policyName);
    EXPECT_TRUE(sameMetricsExceptTime(first.results[p].metrics, second.results[p].metrics));
    ASSERT_EQ(first.results[p].events.size(), second.results[p].events.size());
    for (std::size_t i = 0; i < first.results[p].events.size(); ++i) {
      EXPECT_TRUE(sameEvent(first.results[p].events[i], second.results[p].events[i])) << i;
    }
  }
}

TEST(EngineProperties, PoliciesSeeTheSameKeyOrder) {
  replora::SimulationConfig config;
  config.workload = replora::WorkloadType::Random;
  config.capacity = 3;
  config.traceLength = 40;
  config.seed = 1;
  const replora::SimulationResult result = replora::runSimulation(config);
  expectInvariants(result);
  EXPECT_EQ(result.trace, replora::generateTrace(config.workload, 40, config.seed));
}

TEST(EngineProperties, CapacityCoveringDistinctKeysDoesNotEvict) {
  const replora::SimulationConfig cases[] = {
      {replora::WorkloadType::Looping, 4, 40, 42, replora::PolicySelection::All, "", ""},
      {replora::WorkloadType::Looping, 16, 40, 7, replora::PolicySelection::All, "", ""},
      {replora::WorkloadType::Sequential, 8, 8, 42, replora::PolicySelection::All, "", ""},
      {replora::WorkloadType::Sequential, 16, 8, 42, replora::PolicySelection::All, "", ""},
      {replora::WorkloadType::FrequencySkewed, 16, 50, 42, replora::PolicySelection::All, "", ""},
  };

  for (const replora::SimulationConfig& config : cases) {
    const replora::SimulationResult result = replora::runSimulation(config);
    ASSERT_TRUE(result.errors.empty());
    const std::set<replora::Key> distinct(result.trace.begin(), result.trace.end());
    ASSERT_GE(static_cast<std::size_t>(config.capacity), distinct.size());
    for (const replora::PolicyResult& policy : result.results) {
      EXPECT_EQ(policy.metrics.evictions, 0u) << policy.policyName;
      EXPECT_EQ(policy.metrics.hits, result.trace.size() - distinct.size()) << policy.policyName;
    }
  }
}

TEST(EngineProperties, ExplanationsNameThePolicyStructure) {
  replora::SimulationConfig config;
  config.workload = replora::WorkloadType::Looping;
  config.capacity = 3;
  config.traceLength = 12;
  config.seed = 42;
  const replora::SimulationResult result = replora::runSimulation(config);
  ASSERT_EQ(result.results.size(), 4u);

  for (const replora::PolicyResult& policy : result.results) {
    ASSERT_FALSE(policy.events.empty());
    for (const replora::AccessEvent& event : policy.events) {
      EXPECT_FALSE(event.explanation.empty());
      EXPECT_NE(event.explanation.find(policy.policyName), std::string::npos);
      EXPECT_NE(event.explanation.find(requiredStructure(policy.policyName)), std::string::npos)
          << event.explanation;
    }
  }
  EXPECT_NE(result.results[0].events[0].explanation.find("queue"), std::string::npos);
  EXPECT_NE(result.results[1].events[0].explanation.find("unordered_map"), std::string::npos);
}

}  // namespace
