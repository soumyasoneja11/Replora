#include "replora/random_policy.hpp"
#include "replora/rng.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace {

std::vector<replora::Key> pattern(int length) {
  std::vector<replora::Key> trace;
  trace.reserve(static_cast<std::size_t>(length));
  for (int i = 0; i < length; ++i) {
    trace.push_back(static_cast<replora::Key>(i % 40 + 1));
  }
  return trace;
}

std::vector<replora::Key> victimsOf(std::uint32_t seed, const std::vector<replora::Key>& trace) {
  replora::RandomPolicy policy(8, seed);
  std::vector<replora::Key> victims;
  for (const replora::Key key : trace) {
    const replora::AccessResult result = policy.access(key);
    if (result.evicted) {
      victims.push_back(*result.victim);
    }
  }
  return victims;
}

void expectIndexInvariant(const replora::RandomPolicy& policy) {
  const std::vector<replora::Key>& residents = policy.debugResidents();
  const std::unordered_map<replora::Key, std::size_t>& index = policy.debugIndex();
  ASSERT_EQ(residents.size(), index.size());
  ASSERT_EQ(policy.getCacheState().keys, residents);
  for (std::size_t i = 0; i < residents.size(); ++i) {
    const auto found = index.find(residents[i]);
    ASSERT_NE(found, index.end());
    EXPECT_EQ(found->second, i);
  }
  for (const auto& entry : index) {
    ASSERT_LT(entry.second, residents.size());
    EXPECT_EQ(residents[entry.second], entry.first);
  }
}

TEST(Rng, MatchesTypescriptLcg) {
  replora::Rng rng(42);
  const double first = rng.nextUnit();
  EXPECT_DOUBLE_EQ(first, 1083814273.0 / 4294967296.0);

  replora::Rng again(42);
  EXPECT_DOUBLE_EQ(again.nextUnit(), first);
  again.reset();
  EXPECT_DOUBLE_EQ(again.nextUnit(), first);
}

TEST(RandomPolicy, SameSeedSameVictimsDifferentSeedDiffers) {
  const std::vector<replora::Key> trace = pattern(2000);
  const std::vector<replora::Key> first = victimsOf(42, trace);
  const std::vector<replora::Key> second = victimsOf(42, trace);
  const std::vector<replora::Key> other = victimsOf(7, trace);

  EXPECT_EQ(first, second);
  EXPECT_GT(first.size(), 100u);
  EXPECT_NE(first, other);
}

TEST(RandomPolicy, ResetReplaysIdentically) {
  const std::vector<replora::Key> trace = pattern(300);
  replora::RandomPolicy policy(8, 42);
  std::vector<replora::AccessResult> first;
  for (const replora::Key key : trace) {
    first.push_back(policy.access(key));
  }

  policy.reset();
  EXPECT_TRUE(policy.getCacheState().keys.empty());
  for (std::size_t i = 0; i < trace.size(); ++i) {
    const replora::AccessResult again = policy.access(trace[i]);
    EXPECT_EQ(again.hit, first[i].hit);
    EXPECT_EQ(again.evicted, first[i].evicted);
    EXPECT_EQ(again.victim, first[i].victim);
    EXPECT_EQ(again.reason, first[i].reason);
    EXPECT_EQ(policy.getCacheState().keys, policy.debugResidents());
  }
}

TEST(RandomPolicy, InvariantsHoldOnLongTrace) {
  constexpr std::size_t kCapacity = 5;
  const std::vector<replora::Key> trace = pattern(10000);
  replora::RandomPolicy policy(kCapacity, 42);

  for (const replora::Key key : trace) {
    const std::vector<replora::Key> before = policy.debugResidents();
    const bool wasResident =
        policy.debugIndex().find(key) != policy.debugIndex().end();
    const replora::AccessResult result = policy.access(key);

    if (wasResident) {
      EXPECT_TRUE(result.hit);
      EXPECT_FALSE(result.evicted);
      EXPECT_EQ(result.reason, replora::ReasonCode::Hit);
      EXPECT_EQ(policy.debugResidents(), before);
    } else if (before.size() < kCapacity) {
      EXPECT_FALSE(result.hit);
      EXPECT_FALSE(result.evicted);
      EXPECT_FALSE(result.victim.has_value());
      EXPECT_EQ(result.reason, replora::ReasonCode::MissInsertedFree);
    } else {
      EXPECT_TRUE(result.evicted);
      ASSERT_TRUE(result.victim.has_value());
      EXPECT_NE(std::find(before.begin(), before.end(), *result.victim), before.end());
      EXPECT_EQ(result.reason, replora::ReasonCode::MissEvictedRandomIndex);
    }

    EXPECT_LE(policy.debugResidents().size(), kCapacity);
    EXPECT_EQ(policy.getCacheState().ordering, "arbitrary order (swap-and-pop)");
    expectIndexInvariant(policy);
  }
}

TEST(RandomPolicy, CapacityOne) {
  replora::RandomPolicy policy(1, 42);
  EXPECT_FALSE(policy.access(1).evicted);
  EXPECT_EQ(policy.getCacheState().keys, (std::vector<replora::Key>{1}));
  expectIndexInvariant(policy);

  EXPECT_TRUE(policy.access(1).hit);
  EXPECT_EQ(policy.debugResidents(), (std::vector<replora::Key>{1}));

  const replora::AccessResult evicted = policy.access(2);
  EXPECT_TRUE(evicted.evicted);
  ASSERT_TRUE(evicted.victim.has_value());
  EXPECT_EQ(*evicted.victim, 1);
  EXPECT_EQ(evicted.reason, replora::ReasonCode::MissEvictedRandomIndex);
  EXPECT_EQ(policy.getCacheState().keys, (std::vector<replora::Key>{2}));
  expectIndexInvariant(policy);
}

TEST(RandomPolicy, CapacityZeroThrows) {
  EXPECT_THROW((replora::RandomPolicy{0, 42}), std::invalid_argument);
}

}  // namespace
