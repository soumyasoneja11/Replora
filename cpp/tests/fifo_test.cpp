#include "golden_fixture.hpp"
#include "replora/fifo_policy.hpp"

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <vector>

namespace {

TEST(FifoPolicy, FillsThenEvictsOldest) {
  replora::FifoPolicy fifo(3);

  for (replora::Key key : {1, 2, 3}) {
    const replora::AccessResult result = fifo.access(key);
    EXPECT_FALSE(result.hit);
    EXPECT_FALSE(result.evicted);
    EXPECT_FALSE(result.victim.has_value());
    EXPECT_EQ(result.reason, replora::ReasonCode::MissInsertedFree);
  }

  const replora::CacheState full = fifo.getCacheState();
  EXPECT_EQ(full.ordering, "front=oldest");
  EXPECT_EQ(full.keys, (std::vector<replora::Key>{1, 2, 3}));

  const replora::AccessResult evicted = fifo.access(4);
  EXPECT_FALSE(evicted.hit);
  EXPECT_TRUE(evicted.evicted);
  ASSERT_TRUE(evicted.victim.has_value());
  EXPECT_EQ(*evicted.victim, 1);
  EXPECT_EQ(evicted.reason, replora::ReasonCode::MissEvictedFifoOldest);
  EXPECT_EQ(fifo.getCacheState().keys, (std::vector<replora::Key>{2, 3, 4}));
}

TEST(FifoPolicy, HitDoesNotChangeEvictionOrder) {
  replora::FifoPolicy fifo(3);
  fifo.access(1);
  fifo.access(2);
  fifo.access(3);

  const replora::AccessResult hit = fifo.access(1);
  EXPECT_TRUE(hit.hit);
  EXPECT_FALSE(hit.evicted);
  EXPECT_FALSE(hit.victim.has_value());
  EXPECT_EQ(hit.reason, replora::ReasonCode::Hit);
  EXPECT_EQ(fifo.getCacheState().keys, (std::vector<replora::Key>{1, 2, 3}));

  const replora::AccessResult inserted = fifo.access(4);
  ASSERT_TRUE(inserted.victim.has_value());
  EXPECT_EQ(*inserted.victim, 1);
  EXPECT_EQ(fifo.getCacheState().keys, (std::vector<replora::Key>{2, 3, 4}));
}

TEST(FifoPolicy, RepeatedKeyIsOneMissThenHits) {
  replora::FifoPolicy fifo(3);
  const replora::AccessResult miss = fifo.access(7);
  EXPECT_FALSE(miss.hit);
  EXPECT_EQ(miss.reason, replora::ReasonCode::MissInsertedFree);

  for (int i = 0; i < 5; ++i) {
    const replora::AccessResult hit = fifo.access(7);
    EXPECT_TRUE(hit.hit);
    EXPECT_FALSE(hit.evicted);
    EXPECT_EQ(hit.reason, replora::ReasonCode::Hit);
    EXPECT_EQ(fifo.getCacheState().keys, (std::vector<replora::Key>{7}));
  }
}

TEST(FifoPolicy, CapacityOneEvictsOnEveryMiss) {
  replora::FifoPolicy fifo(1);

  const replora::AccessResult first = fifo.access(1);
  EXPECT_FALSE(first.evicted);
  EXPECT_EQ(fifo.getCacheState().keys, (std::vector<replora::Key>{1}));

  const replora::AccessResult second = fifo.access(2);
  EXPECT_TRUE(second.evicted);
  ASSERT_TRUE(second.victim.has_value());
  EXPECT_EQ(*second.victim, 1);
  EXPECT_EQ(second.reason, replora::ReasonCode::MissEvictedFifoOldest);
  EXPECT_EQ(fifo.getCacheState().keys, (std::vector<replora::Key>{2}));

  const replora::AccessResult hit = fifo.access(2);
  EXPECT_TRUE(hit.hit);
  EXPECT_EQ(fifo.getCacheState().keys, (std::vector<replora::Key>{2}));

  const replora::AccessResult third = fifo.access(3);
  ASSERT_TRUE(third.victim.has_value());
  EXPECT_EQ(*third.victim, 2);
  EXPECT_EQ(fifo.getCacheState().keys, (std::vector<replora::Key>{3}));
}

TEST(FifoPolicy, CapacityZeroThrows) {
  EXPECT_THROW(replora::FifoPolicy{0}, std::invalid_argument);
}

TEST(FifoPolicy, ResetRestoresFreshState) {
  replora::FifoPolicy fifo(2);
  fifo.access(1);
  fifo.access(2);
  fifo.access(3);
  fifo.reset();

  EXPECT_TRUE(fifo.getCacheState().keys.empty());
  EXPECT_EQ(fifo.getCacheState().ordering, "front=oldest");

  const replora::AccessResult again = fifo.access(1);
  EXPECT_FALSE(again.hit);
  EXPECT_FALSE(again.evicted);
  EXPECT_EQ(again.reason, replora::ReasonCode::MissInsertedFree);
  EXPECT_EQ(fifo.getCacheState().keys, (std::vector<replora::Key>{1}));
}

TEST(FifoPolicy, MatchesGoldenBurstyTrace) {
  const std::string path = std::string(REPLORA_GOLDEN_DIR) + "/bursty_c4_n20_s42.json";
  const replora::test::GoldenPolicyRun golden = replora::test::loadGoldenPolicy(path, "FIFO");
  ASSERT_EQ(golden.trace.size(), golden.steps.size());

  replora::FifoPolicy fifo(static_cast<std::size_t>(golden.capacity));
  std::uint64_t hits = 0;
  std::uint64_t misses = 0;
  std::uint64_t evictions = 0;

  for (std::size_t i = 0; i < golden.trace.size(); ++i) {
    const replora::AccessResult result = fifo.access(golden.trace[i]);
    const replora::test::GoldenStep& step = golden.steps[i];

    EXPECT_EQ(result.hit, step.hit) << "step " << i;
    EXPECT_EQ(result.evicted, step.evicted) << "step " << i;
    EXPECT_EQ(result.victim, step.victim) << "step " << i;
    EXPECT_EQ(fifo.getCacheState().keys, step.cacheStateAfter) << "step " << i;
    EXPECT_EQ(fifo.getCacheState().ordering, "front=oldest");

    if (step.hit) {
      EXPECT_EQ(result.reason, replora::ReasonCode::Hit);
      ++hits;
    } else if (step.evicted) {
      EXPECT_EQ(result.reason, replora::ReasonCode::MissEvictedFifoOldest);
      ++misses;
      ++evictions;
    } else {
      EXPECT_EQ(result.reason, replora::ReasonCode::MissInsertedFree);
      ++misses;
    }
  }

  EXPECT_EQ(hits, golden.hits);
  EXPECT_EQ(misses, golden.misses);
  EXPECT_EQ(evictions, golden.evictions);
}

}  // namespace
