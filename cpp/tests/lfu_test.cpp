#include "golden_fixture.hpp"
#include "replora/lfu_policy.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

TEST(LfuPolicy, EvictsLowestFrequency) {
  replora::LfuPolicy lfu(2);
  lfu.access(1);
  lfu.access(1);
  lfu.access(2);

  const replora::AccessResult evicted = lfu.access(3);
  EXPECT_TRUE(evicted.evicted);
  ASSERT_TRUE(evicted.victim.has_value());
  EXPECT_EQ(*evicted.victim, 2);
  EXPECT_EQ(evicted.reason, replora::ReasonCode::MissEvictedLfuMinFreq);

  const replora::CacheState state = lfu.getCacheState();
  EXPECT_EQ(state.ordering, "insertion order");
  EXPECT_EQ(state.keys, (std::vector<replora::Key>{1, 3}));
  EXPECT_EQ(state.frequencies, (std::vector<std::uint64_t>{2, 1}));
}

TEST(LfuPolicy, TieEvictsEarliestInsertion) {
  replora::LfuPolicy lfu(2);
  lfu.access(1);
  lfu.access(2);

  const replora::AccessResult evicted = lfu.access(3);
  ASSERT_TRUE(evicted.victim.has_value());
  EXPECT_EQ(*evicted.victim, 1);
  EXPECT_EQ(lfu.getCacheState().keys, (std::vector<replora::Key>{2, 3}));
  EXPECT_EQ(lfu.getCacheState().frequencies, (std::vector<std::uint64_t>{1, 1}));
}

TEST(LfuPolicy, ReinsertResetsFrequency) {
  replora::LfuPolicy lfu(2);
  lfu.access(1);
  lfu.access(1);
  lfu.access(1);
  lfu.access(2);
  ASSERT_EQ(*lfu.access(3).victim, 2);

  lfu.access(3);
  lfu.access(3);
  ASSERT_EQ(*lfu.access(4).victim, 1);

  ASSERT_EQ(*lfu.access(1).victim, 4);
  EXPECT_EQ(lfu.getCacheState().keys, (std::vector<replora::Key>{3, 1}));
  EXPECT_EQ(lfu.getCacheState().frequencies, (std::vector<std::uint64_t>{3, 1}));

  const replora::AccessResult next = lfu.access(5);
  ASSERT_TRUE(next.victim.has_value());
  EXPECT_EQ(*next.victim, 1);
}

TEST(LfuPolicy, HitsProtectAKey) {
  replora::LfuPolicy lfu(2);
  lfu.access(1);
  lfu.access(2);
  const replora::AccessResult hit = lfu.access(1);
  EXPECT_TRUE(hit.hit);
  EXPECT_EQ(hit.reason, replora::ReasonCode::Hit);
  EXPECT_EQ(lfu.getCacheState().keys, (std::vector<replora::Key>{1, 2}));
  EXPECT_EQ(lfu.getCacheState().frequencies, (std::vector<std::uint64_t>{2, 1}));

  const replora::AccessResult evicted = lfu.access(3);
  ASSERT_TRUE(evicted.victim.has_value());
  EXPECT_EQ(*evicted.victim, 2);
  EXPECT_EQ(lfu.getCacheState().keys, (std::vector<replora::Key>{1, 3}));
}

TEST(LfuPolicy, CapacityOne) {
  replora::LfuPolicy lfu(1);
  EXPECT_FALSE(lfu.access(1).evicted);
  EXPECT_TRUE(lfu.access(1).hit);
  EXPECT_EQ(lfu.getCacheState().frequencies, (std::vector<std::uint64_t>{2}));

  const replora::AccessResult evicted = lfu.access(2);
  ASSERT_TRUE(evicted.victim.has_value());
  EXPECT_EQ(*evicted.victim, 1);
  EXPECT_EQ(evicted.reason, replora::ReasonCode::MissEvictedLfuMinFreq);
  EXPECT_EQ(lfu.getCacheState().keys, (std::vector<replora::Key>{2}));
  EXPECT_EQ(lfu.getCacheState().frequencies, (std::vector<std::uint64_t>{1}));
}

TEST(LfuPolicy, CapacityZeroThrows) {
  EXPECT_THROW(replora::LfuPolicy{0}, std::invalid_argument);
}

TEST(LfuPolicy, ResetRestoresFreshState) {
  replora::LfuPolicy lfu(2);
  lfu.access(1);
  lfu.access(1);
  lfu.access(2);
  lfu.access(3);
  lfu.reset();

  EXPECT_TRUE(lfu.getCacheState().keys.empty());
  EXPECT_TRUE(lfu.getCacheState().frequencies.empty());

  const replora::AccessResult again = lfu.access(1);
  EXPECT_FALSE(again.hit);
  EXPECT_FALSE(again.evicted);
  EXPECT_EQ(again.reason, replora::ReasonCode::MissInsertedFree);
  EXPECT_EQ(lfu.getCacheState().frequencies, (std::vector<std::uint64_t>{1}));
}

TEST(LfuPolicy, MatchesEveryGoldenFile) {
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
        replora::test::loadGoldenPolicy(path.string(), "LFU");
    ASSERT_EQ(golden.trace.size(), golden.steps.size()) << path;

    replora::LfuPolicy lfu(static_cast<std::size_t>(golden.capacity));
    std::uint64_t hits = 0;
    std::uint64_t misses = 0;
    std::uint64_t evictions = 0;

    for (std::size_t i = 0; i < golden.trace.size(); ++i) {
      const replora::AccessResult result = lfu.access(golden.trace[i]);
      const replora::test::GoldenStep& step = golden.steps[i];

      EXPECT_EQ(result.hit, step.hit) << path << " step " << i;
      EXPECT_EQ(result.evicted, step.evicted) << path << " step " << i;
      EXPECT_EQ(result.victim, step.victim) << path << " step " << i;
      EXPECT_EQ(lfu.getCacheState().keys, step.cacheStateAfter) << path << " step " << i;
      EXPECT_EQ(lfu.getCacheState().ordering, "insertion order");
      EXPECT_EQ(lfu.getCacheState().frequencies.size(), step.cacheStateAfter.size());

      if (step.hit) {
        EXPECT_EQ(result.reason, replora::ReasonCode::Hit);
        ++hits;
      } else if (step.evicted) {
        EXPECT_EQ(result.reason, replora::ReasonCode::MissEvictedLfuMinFreq);
        ++misses;
        ++evictions;
      } else {
        EXPECT_EQ(result.reason, replora::ReasonCode::MissInsertedFree);
        ++misses;
      }
    }

    EXPECT_EQ(hits, golden.hits) << path;
    EXPECT_EQ(misses, golden.misses) << path;
    EXPECT_EQ(evictions, golden.evictions) << path;
  }
}

}  // namespace
