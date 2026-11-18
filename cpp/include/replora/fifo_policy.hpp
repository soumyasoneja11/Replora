#pragma once

#include "replora/cache_policy.hpp"

#include <queue>
#include <unordered_set>

namespace replora {

// FIFO: std::queue (oldest at front) plus unordered_set for membership.
// A hit does not reorder. Capacity < 1 throws std::invalid_argument.
class FifoPolicy final : public CachePolicy {
 public:
  explicit FifoPolicy(std::size_t capacity);

  AccessResult access(Key key) override;
  void reset() override;
  std::string getName() const override;
  CacheState getCacheState() const override;

  // Estimate, not RSS. See fifo_policy.cpp for the formula.
  std::size_t estimateMemoryBytes() const override;

 private:
  std::queue<Key> order_;
  std::unordered_set<Key> member_;
};

}  // namespace replora
