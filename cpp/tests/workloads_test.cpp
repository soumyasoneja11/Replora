#include "golden_fixture.hpp"
#include "replora/workloads.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

namespace {

TEST(Workloads, MatchesEveryGoldenTrace) {
  std::vector<std::filesystem::path> files;
  for (const std::filesystem::directory_entry& entry :
       std::filesystem::directory_iterator(REPLORA_GOLDEN_DIR)) {
    if (entry.path().extension() == ".json") {
      files.push_back(entry.path());
    }
  }
  std::sort(files.begin(), files.end());
  ASSERT_EQ(files.size(), 160u);

  for (const std::filesystem::path& path : files) {
    const replora::test::GoldenPolicyRun golden =
        replora::test::loadGoldenPolicy(path.string(), "FIFO");
    const std::optional<replora::WorkloadType> workload =
        replora::workloadFromString(golden.workload);
    ASSERT_TRUE(workload.has_value()) << path << " workload " << golden.workload;

    const std::vector<replora::Key> trace =
        replora::generateTrace(*workload, static_cast<std::size_t>(golden.traceLength), golden.seed);
    EXPECT_EQ(trace.size(), static_cast<std::size_t>(golden.traceLength)) << path;
    EXPECT_EQ(trace, golden.trace) << path;
  }
}

TEST(Workloads, LengthZeroIsEmptyAndRunsAreDeterministic) {
  const replora::WorkloadType workloads[] = {
      replora::WorkloadType::Sequential, replora::WorkloadType::Random,
      replora::WorkloadType::Looping,    replora::WorkloadType::Bursty,
      replora::WorkloadType::FrequencySkewed,
  };
  for (const replora::WorkloadType workload : workloads) {
    EXPECT_TRUE(replora::generateTrace(workload, 0, 42).empty());
    const std::vector<replora::Key> first = replora::generateTrace(workload, 100, 42);
    const std::vector<replora::Key> second = replora::generateTrace(workload, 100, 42);
    EXPECT_EQ(first.size(), 100u);
    EXPECT_EQ(first, second);
  }
}

TEST(Workloads, UnknownNameIsAnError) {
  EXPECT_FALSE(replora::workloadFromString("").has_value());
  EXPECT_FALSE(replora::workloadFromString("not-a-workload").has_value());
  EXPECT_FALSE(replora::workloadFromString("FrequencySkewed").has_value());
}

TEST(Workloads, SeedChangesOnlyTheRandomizedWorkloads) {
  EXPECT_NE(replora::generateTrace(replora::WorkloadType::Random, 100, 42),
            replora::generateTrace(replora::WorkloadType::Random, 100, 7));
  EXPECT_NE(replora::generateTrace(replora::WorkloadType::Bursty, 100, 42),
            replora::generateTrace(replora::WorkloadType::Bursty, 100, 7));
  EXPECT_NE(replora::generateTrace(replora::WorkloadType::FrequencySkewed, 100, 42),
            replora::generateTrace(replora::WorkloadType::FrequencySkewed, 100, 7));
  EXPECT_EQ(replora::generateTrace(replora::WorkloadType::Sequential, 100, 42),
            replora::generateTrace(replora::WorkloadType::Sequential, 100, 7));
  EXPECT_EQ(replora::generateTrace(replora::WorkloadType::Looping, 100, 42),
            replora::generateTrace(replora::WorkloadType::Looping, 100, 7));
}

TEST(Workloads, SequentialIncreasesAndLoopingRepeats) {
  const std::vector<replora::Key> sequential =
      replora::generateTrace(replora::WorkloadType::Sequential, 20, 42);
  for (std::size_t i = 0; i < sequential.size(); ++i) {
    EXPECT_EQ(sequential[i], static_cast<replora::Key>(i + 1));
  }

  const std::vector<replora::Key> looping =
      replora::generateTrace(replora::WorkloadType::Looping, 20, 42);
  const replora::Key period[] = {1, 2, 3, 4};
  for (std::size_t i = 0; i < looping.size(); ++i) {
    EXPECT_EQ(looping[i], period[i % 4]);
    if (i + 4 < looping.size()) {
      EXPECT_EQ(looping[i], looping[i + 4]);
    }
  }
}

}  // namespace
