#include "replora/lfu_policy.hpp"

#include <algorithm>
#include <utility>
#include <vector>

namespace replora {

LfuPolicy::LfuPolicy(std::size_t capacity) : CachePolicy(capacity) {}

AccessResult LfuPolicy::access(Key key) {
  // Lookup O(1) average. A hit only increments frequency.
  const auto found = index_.find(key);
  if (found != index_.end()) {
    ++found->second.frequency;
    return AccessResult{true, false, std::nullopt, ReasonCode::Hit};
  }

  std::optional<Key> victim;
  ReasonCode reason = ReasonCode::MissInsertedFree;
  if (index_.size() >= capacity_) {
    // Eviction O(C): scan every resident. Lowest frequency wins.
    // Equal frequencies: smallest insertion sequence (earliest insert).
    auto chosen = index_.begin();
    for (auto it = index_.begin(); it != index_.end(); ++it) {
      const Entry& candidate = it->second;
      const Entry& best = chosen->second;
      if (candidate.frequency < best.frequency ||
          (candidate.frequency == best.frequency && candidate.insertedAt < best.insertedAt)) {
        chosen = it;
      }
    }
    victim = chosen->first;
    index_.erase(chosen);
    reason = ReasonCode::MissEvictedLfuMinFreq;
  }

  // O(1) average. A new stay always starts at frequency 1 with a fresh sequence.
  index_.emplace(key, Entry{1, nextInsertion_++});
  return AccessResult{false, victim.has_value(), victim, reason};
}

void LfuPolicy::reset() {
  index_.clear();
  nextInsertion_ = 0;
}

std::string LfuPolicy::getName() const { return "LFU"; }

CacheState LfuPolicy::getCacheState() const {
  // Display order is insertion sequence, matching the TypeScript items array.
  // Sorting is for the snapshot only. Eviction itself is the linear scan above.
  struct Row {
    std::uint64_t insertedAt;
    Key key;
    std::uint64_t frequency;
  };
  std::vector<Row> rows;
  rows.reserve(index_.size());
  for (const auto& [key, entry] : index_) {
    rows.push_back(Row{entry.insertedAt, key, entry.frequency});
  }
  std::sort(rows.begin(), rows.end(), [](const Row& left, const Row& right) {
    return left.insertedAt < right.insertedAt;
  });

  CacheState state;
  state.ordering = "insertion order";
  state.keys.reserve(rows.size());
  state.frequencies.reserve(rows.size());
  for (const Row& row : rows) {
    state.keys.push_back(row.key);
    state.frequencies.push_back(row.frequency);
  }
  return state;
}

// Heap estimate, never process RSS. Bucket padding beyond the pointer array is ignored.
//   sizeof(LfuPolicy)                                      empty map and counter
//   + residents * (sizeof(Key) + sizeof(Entry) + sizeof(void*))
//                                                          map node: key, entry, next
//   + bucket_count * sizeof(void*)                         bucket array
std::size_t LfuPolicy::estimateMemoryBytes() const {
  const std::size_t residents = index_.size();
  const std::size_t node = sizeof(Key) + sizeof(Entry) + sizeof(void*);
  return sizeof(LfuPolicy) + residents * node + index_.bucket_count() * sizeof(void*);
}

}  // namespace replora
