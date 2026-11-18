#include "replora/engine.hpp"
#include "replora/json.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <iostream>
#include <string>

namespace {

TEST(EngineProperties, LargeRunReportsJsonSize) {
  replora::SimulationConfig config;
  config.workload = replora::WorkloadType::Looping;
  config.capacity = 4;
  config.traceLength = 10000;
  config.seed = 42;

  const auto started = std::chrono::steady_clock::now();
  const replora::SimulationResult result = replora::runSimulation(config);
  const std::string json = replora::toJson(result);
  const auto finished = std::chrono::steady_clock::now();
  const auto elapsedMs =
      std::chrono::duration_cast<std::chrono::milliseconds>(finished - started).count();

  ASSERT_TRUE(result.errors.empty());
  ASSERT_EQ(result.results.size(), 4u);
  EXPECT_EQ(result.trace.size(), 10000u);
  EXPECT_LT(elapsedMs, 60000);
  std::cout << "JSON bytes: " << json.size() << " elapsed ms: " << elapsedMs << std::endl;
  EXPECT_GT(json.size(), 10000u);
}

}  // namespace
