#include "golden_fixture.hpp"
#include "replora/fifo_policy.hpp"
#include "replora/lru_policy.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void expectSizesMatch(const replora::LruPolicy& lru) {
  EXPECT_EQ(lru.debugListSize(), lru.debugMapSize());
  EXPECT_EQ(lru.debugListSize(), lru.getCacheState().keys.size());
}

TEST(LruPolicy, HitPromotesToMru) {
  replora::LruPolicy lru(3);
  lru.access(1);
  lru.access(2);
  lru.access(3);
  expectSizesMatch(lru);

  const replora::AccessResult hit = lru.access(1);
  EXPECT_TRUE(hit.hit);
  EXPECT_FALSE(hit.evicted);
  EXPECT_EQ(hit.reason, replora::ReasonCode::Hit);
  EXPECT_EQ(lru.getCacheState().keys, (std::vector<replora::Key>{1, 3, 2}));
  EXPECT_EQ(lru.getCacheState().ordering, "front=MRU, back=LRU");
  expectSizesMatch(lru);

  const replora::AccessResult inserted = lru.access(4);
  EXPECT_TRUE(inserted.evicted);
  ASSERT_TRUE(inserted.victim.has_value());
  EXPECT_EQ(*inserted.victim, 2);
  EXPECT_EQ(inserted.reason, replora::ReasonCode::MissEvictedLruTail);
  EXPECT_EQ(lru.getCacheState().keys, (std::vector<replora::Key>{4, 1, 3}));
  expectSizesMatch(lru);
}

TEST(LruPolicy, FirstVictimDiffersFromFifoOnSharedTrace) {
  const std::vector<replora::Key> trace = {1, 2, 3, 1, 4};

  replora::FifoPolicy fifo(3);
  replora::LruPolicy lru(3);
  std::optional<replora::Key> fifoVictim;
  std::optional<replora::Key> lruVictim;

  for (const replora::Key key : trace) {
    const replora::AccessResult fifoResult = fifo.access(key);
    const replora::AccessResult lruResult = lru.access(key);
    expectSizesMatch(lru);
    if (fifoResult.evicted && !fifoVictim.has_value()) {
      fifoVictim = fifoResult.victim;
    }
    if (lruResult.evicted && !lruVictim.has_value()) {
      lruVictim = lruResult.victim;
    }
  }

  ASSERT_TRUE(fifoVictim.has_value());
  ASSERT_TRUE(lruVictim.has_value());
  EXPECT_EQ(*fifoVictim, 1);
  EXPECT_EQ(*lruVictim, 2);
  EXPECT_NE(*fifoVictim, *lruVictim);
}

TEST(LruPolicy, CapacityOne) {
  replora::LruPolicy lru(1);
  const replora::AccessResult first = lru.access(1);
  EXPECT_FALSE(first.evicted);
  EXPECT_EQ(first.reason, replora::ReasonCode::MissInsertedFree);
  EXPECT_EQ(lru.getCacheState().keys, (std::vector<replora::Key>{1}));

  const replora::AccessResult hit = lru.access(1);
  EXPECT_TRUE(hit.hit);
  EXPECT_EQ(lru.getCacheState().keys, (std::vector<replora::Key>{1}));

  const replora::AccessResult second = lru.access(2);
  EXPECT_TRUE(second.evicted);
  ASSERT_TRUE(second.victim.has_value());
  EXPECT_EQ(*second.victim, 1);
  EXPECT_EQ(second.reason, replora::ReasonCode::MissEvictedLruTail);
  EXPECT_EQ(lru.getCacheState().keys, (std::vector<replora::Key>{2}));
  expectSizesMatch(lru);
}

TEST(LruPolicy, CapacityZeroThrows) {
  EXPECT_THROW(replora::LruPolicy{0}, std::invalid_argument);
}

TEST(LruPolicy, ResetRestoresFreshState) {
  replora::LruPolicy lru(2);
  lru.access(1);
  lru.access(2);
  lru.access(1);
  lru.access(3);
  lru.reset();

  EXPECT_TRUE(lru.getCacheState().keys.empty());
  EXPECT_EQ(lru.debugListSize(), 0u);
  EXPECT_EQ(lru.debugMapSize(), 0u);

  const replora::AccessResult again = lru.access(1);
  EXPECT_FALSE(again.hit);
  EXPECT_FALSE(again.evicted);
  EXPECT_EQ(again.reason, replora::ReasonCode::MissInsertedFree);
  EXPECT_EQ(lru.getCacheState().keys, (std::vector<replora::Key>{1}));
  expectSizesMatch(lru);
}

TEST(LruPolicy, MatchesEveryGoldenFile) {
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
        replora::test::loadGoldenPolicy(path.string(), "LRU");
    ASSERT_EQ(golden.trace.size(), golden.steps.size()) << path;

    replora::LruPolicy lru(static_cast<std::size_t>(golden.capacity));
    std::uint64_t hits = 0;
    std::uint64_t misses = 0;
    std::uint64_t evictions = 0;

    for (std::size_t i = 0; i < golden.trace.size(); ++i) {
      const replora::AccessResult result = lru.access(golden.trace[i]);
      const replora::test::GoldenStep& step = golden.steps[i];

      EXPECT_EQ(result.hit, step.hit) << path << " step " << i;
      EXPECT_EQ(result.evicted, step.evicted) << path << " step " << i;
      EXPECT_EQ(result.victim, step.victim) << path << " step " << i;

      // Golden cacheStateAfter is the TypeScript order: index 0 is LRU.
      // This list walks front to back, which is MRU to LRU.
      std::vector<replora::Key> mruFirst = step.cacheStateAfter;
      std::reverse(mruFirst.begin(), mruFirst.end());
      EXPECT_EQ(lru.getCacheState().keys, mruFirst) << path << " step " << i;
      EXPECT_EQ(lru.getCacheState().ordering, "front=MRU, back=LRU");
      expectSizesMatch(lru);

      if (step.hit) {
        EXPECT_EQ(result.reason, replora::ReasonCode::Hit);
        ++hits;
      } else if (step.evicted) {
        EXPECT_EQ(result.reason, replora::ReasonCode::MissEvictedLruTail);
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
