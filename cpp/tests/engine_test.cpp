#include "golden_fixture.hpp"
#include "replora/engine.hpp"
#include "replora/json.hpp"
#include "replora/workloads.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

namespace {

void expectConsistent(const replora::SimulationResult& result) {
  ASSERT_TRUE(result.errors.empty());
  EXPECT_EQ(result.trace.size(), static_cast<std::size_t>(result.config.traceLength));
  for (const replora::PolicyResult& policy : result.results) {
    std::uint64_t hits = 0;
    std::uint64_t misses = 0;
    std::uint64_t evictions = 0;
    for (const replora::AccessEvent& event : policy.events) {
      if (event.hit) {
        ++hits;
      } else {
        ++misses;
      }
      if (event.evicted) {
        ++evictions;
      }
      EXPECT_FALSE(event.explanation.empty());
    }
    EXPECT_EQ(hits + misses, static_cast<std::uint64_t>(result.config.traceLength));
    EXPECT_EQ(policy.metrics.hits, hits);
    EXPECT_EQ(policy.metrics.misses, misses);
    EXPECT_EQ(policy.metrics.evictions, evictions);
    EXPECT_EQ(policy.metrics.totalAccesses, hits + misses);
    if (result.config.traceLength == 0) {
      EXPECT_DOUBLE_EQ(policy.metrics.hitRate, 0.0);
      EXPECT_DOUBLE_EQ(policy.metrics.missRate, 0.0);
    } else {
      EXPECT_NEAR(policy.metrics.hitRate + policy.metrics.missRate, 100.0, 1e-9);
    }
  }
}

std::vector<replora::Key> victimsOf(const replora::PolicyResult& policy) {
  std::vector<replora::Key> victims;
  for (const replora::AccessEvent& event : policy.events) {
    if (event.evicted && event.victim.has_value()) {
      victims.push_back(*event.victim);
    }
  }
  return victims;
}

bool jsonIsBalanced(const std::string& text) {
  int braces = 0;
  int brackets = 0;
  bool inString = false;
  bool escaped = false;
  for (const char character : text) {
    if (inString) {
      if (escaped) {
        escaped = false;
        continue;
      }
      if (character == '\\') {
        escaped = true;
        continue;
      }
      if (character == '"') {
        inString = false;
      }
      continue;
    }
    if (character == '"') {
      inString = true;
      continue;
    }
    if (character == '{') {
      ++braces;
    } else if (character == '}') {
      if (--braces < 0) {
        return false;
      }
    } else if (character == '[') {
      ++brackets;
    } else if (character == ']') {
      if (--brackets < 0) {
        return false;
      }
    }
  }
  return !inString && !escaped && braces == 0 && brackets == 0;
}

TEST(Engine, AllPoliciesShareOneTrace) {
  replora::SimulationConfig config;
  config.workload = replora::WorkloadType::Looping;
  config.capacity = 4;
  config.traceLength = 20;
  config.seed = 42;

  const replora::SimulationResult result = replora::runSimulation(config);
  expectConsistent(result);
  ASSERT_EQ(result.results.size(), 4u);
  EXPECT_EQ(result.trace, replora::generateTrace(config.workload, 20, config.seed));
  for (const replora::PolicyResult& policy : result.results) {
    ASSERT_EQ(policy.events.size(), result.trace.size());
    for (std::size_t i = 0; i < result.trace.size(); ++i) {
      EXPECT_EQ(policy.events[i].key, result.trace[i]);
      EXPECT_EQ(policy.events[i].policyName, policy.policyName);
    }
  }
}

TEST(Engine, FreeSpaceMissDoesNotEvict) {
  replora::SimulationConfig config;
  config.workload = replora::WorkloadType::Sequential;
  config.capacity = 4;
  config.traceLength = 2;
  config.seed = 42;

  const replora::SimulationResult result = replora::runSimulation(config);
  expectConsistent(result);
  for (const replora::PolicyResult& policy : result.results) {
    ASSERT_EQ(policy.events.size(), 2u);
    EXPECT_FALSE(policy.events[0].hit);
    EXPECT_FALSE(policy.events[0].evicted);
    EXPECT_FALSE(policy.events[1].evicted);
    EXPECT_EQ(policy.events[1].stateAfter.keys.size(), 2u);
    EXPECT_EQ(policy.metrics.evictions, 0u);
  }
}

TEST(Engine, FifoAndLruVictimsDiffer) {
  replora::SimulationConfig config;
  config.workload = replora::WorkloadType::FrequencySkewed;
  config.capacity = 4;
  config.traceLength = 80;
  config.seed = 42;

  const replora::SimulationResult result = replora::runSimulation(config);
  expectConsistent(result);
  const replora::PolicyResult* fifo = nullptr;
  const replora::PolicyResult* lru = nullptr;
  for (const replora::PolicyResult& policy : result.results) {
    if (policy.policyName == "FIFO") {
      fifo = &policy;
    }
    if (policy.policyName == "LRU") {
      lru = &policy;
    }
  }
  ASSERT_NE(fifo, nullptr);
  ASSERT_NE(lru, nullptr);
  EXPECT_NE(victimsOf(*fifo), victimsOf(*lru));
}

TEST(Engine, LengthZeroHasZeroRates) {
  replora::SimulationConfig config;
  config.traceLength = 0;
  config.capacity = 4;
  const replora::SimulationResult result = replora::runSimulation(config);
  expectConsistent(result);
  EXPECT_TRUE(result.trace.empty());
  EXPECT_EQ(result.results.size(), 4u);
}

TEST(Engine, CapacityOneRuns) {
  replora::SimulationConfig config;
  config.workload = replora::WorkloadType::Sequential;
  config.capacity = 1;
  config.traceLength = 4;
  const replora::SimulationResult result = replora::runSimulation(config);
  expectConsistent(result);
  for (const replora::PolicyResult& policy : result.results) {
    EXPECT_EQ(policy.metrics.evictions, 3u);
    EXPECT_EQ(policy.events.back().stateAfter.keys.size(), 1u);
  }
}

TEST(Engine, InvalidConfigReturnsErrors) {
  replora::SimulationConfig capacity;
  capacity.capacity = 0;
  const replora::SimulationResult badCapacity = replora::runSimulation(capacity);
  ASSERT_FALSE(badCapacity.errors.empty());
  EXPECT_EQ(badCapacity.errors[0].field, "capacity");
  EXPECT_TRUE(badCapacity.results.empty());

  replora::SimulationConfig negative;
  negative.traceLength = -3;
  const replora::SimulationResult badLength = replora::runSimulation(negative);
  ASSERT_FALSE(badLength.errors.empty());
  EXPECT_EQ(badLength.errors[0].field, "traceLength");

  replora::SimulationConfig tooLong;
  tooLong.traceLength = replora::kMaxTraceLength + 1;
  const replora::SimulationResult overLimit = replora::runSimulation(tooLong);
  ASSERT_FALSE(overLimit.errors.empty());
  EXPECT_EQ(overLimit.errors[0].field, "traceLength");

  replora::SimulationConfig workload;
  workload.workloadName = "not-a-workload";
  const replora::SimulationResult unknownWorkload = replora::runSimulation(workload);
  ASSERT_FALSE(unknownWorkload.errors.empty());
  EXPECT_EQ(unknownWorkload.errors[0].field, "workload");

  replora::SimulationConfig policy;
  policy.policySelection = replora::PolicySelection::One;
  policy.policyName = "MRU";
  const replora::SimulationResult unknownPolicy = replora::runSimulation(policy);
  ASSERT_FALSE(unknownPolicy.errors.empty());
  EXPECT_EQ(unknownPolicy.errors[0].field, "policy");
}

TEST(Engine, GoldenFifoLruLfuMatch) {
  std::vector<std::filesystem::path> files;
  for (const std::filesystem::directory_entry& entry :
       std::filesystem::directory_iterator(REPLORA_GOLDEN_DIR)) {
    if (entry.path().extension() == ".json") {
      files.push_back(entry.path());
    }
  }
  std::sort(files.begin(), files.end());
  ASSERT_EQ(files.size(), 160u);

  const char* policies[] = {"FIFO", "LRU", "LFU"};
  for (const std::filesystem::path& path : files) {
    const replora::test::GoldenPolicyRun sample =
        replora::test::loadGoldenPolicy(path.string(), "FIFO");
    for (const char* policyName : policies) {
      const replora::test::GoldenPolicyRun golden =
          replora::test::loadGoldenPolicy(path.string(), policyName);
      replora::SimulationConfig config;
      config.workload = *replora::workloadFromString(sample.workload);
      config.capacity = static_cast<std::int64_t>(sample.capacity);
      config.traceLength = static_cast<std::int64_t>(sample.traceLength);
      config.seed = sample.seed;
      config.policySelection = replora::PolicySelection::One;
      config.policyName = policyName;

      const replora::SimulationResult result = replora::runSimulation(config);
      ASSERT_TRUE(result.errors.empty()) << path;
      ASSERT_EQ(result.results.size(), 1u) << path;
      ASSERT_EQ(result.trace, golden.trace) << path << " " << policyName;
      ASSERT_EQ(result.results[0].events.size(), golden.steps.size()) << path;

      for (std::size_t i = 0; i < golden.steps.size(); ++i) {
        const replora::AccessEvent& event = result.results[0].events[i];
        const replora::test::GoldenStep& step = golden.steps[i];
        EXPECT_EQ(event.hit, step.hit) << path << " " << policyName << " step " << i;
        EXPECT_EQ(event.evicted, step.evicted) << path << " " << policyName << " step " << i;
        EXPECT_EQ(event.victim, step.victim) << path << " " << policyName << " step " << i;
        std::vector<replora::Key> expected = step.cacheStateAfter;
        if (std::string(policyName) == "LRU") {
          std::reverse(expected.begin(), expected.end());
        }
        EXPECT_EQ(event.stateAfter.keys, expected) << path << " " << policyName << " step " << i;
      }
    }
  }
}

TEST(Engine, JsonIsBalancedAndEscapesQuotes) {
  replora::SimulationConfig config;
  config.workload = replora::WorkloadType::Looping;
  config.capacity = 2;
  config.traceLength = 6;
  config.seed = 42;
  const replora::SimulationResult result = replora::runSimulation(config);
  expectConsistent(result);
  const std::string json = replora::toJson(result);
  EXPECT_TRUE(jsonIsBalanced(json));
  EXPECT_NE(json.find("\\\""), std::string::npos);
  EXPECT_NE(result.results[0].events[0].explanation.find('"'), std::string::npos);
}

}  // namespace
