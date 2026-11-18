#pragma once

#include "replora/cache_policy.hpp"
#include "replora/rng.hpp"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace replora {

// Random: vector of residents plus unordered_map<key, index>.
// A full-cache miss evicts a random slot with swap-and-pop.
// Capacity < 1 throws std::invalid_argument.
class RandomPolicy final : public CachePolicy {
 public:
  RandomPolicy(std::size_t capacity, std::uint32_t seed);

  AccessResult access(Key key) override;
  void reset() override;
  std::string getName() const override;
  CacheState getCacheState() const override;
  std::size_t estimateMemoryBytes() const override;

  const std::vector<Key>& debugResidents() const { return residents_; }
  const std::unordered_map<Key, std::size_t>& debugIndex() const { return index_; }

 private:
  std::vector<Key> residents_;
  std::unordered_map<Key, std::size_t> index_;
  Rng rng_;
};

}  // namespace replora
