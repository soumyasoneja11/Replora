#pragma once

#include "replora/cache_policy.hpp"

#include <cstdint>
#include <unordered_map>

namespace replora {

// LFU: one hash entry per resident key. Eviction scans every entry.
// No heap, frequency buckets, or frequency lists.
//
// src/algorithms/lfu.ts deletes freq and insertionOrder on eviction, then
// assigns frequency 1 and a new insertionCounter if the key is inserted again.
// Frequency is not remembered after the key leaves the cache.
class LfuPolicy final : public CachePolicy {
 public:
  explicit LfuPolicy(std::size_t capacity);

  AccessResult access(Key key) override;
  void reset() override;
  std::string getName() const override;
  CacheState getCacheState() const override;
  std::size_t estimateMemoryBytes() const override;

 private:
  struct Entry {
    std::uint64_t frequency = 0;
    std::uint64_t insertedAt = 0;
  };

  std::unordered_map<Key, Entry> index_;
  std::uint64_t nextInsertion_ = 0;
};

}  // namespace replora
