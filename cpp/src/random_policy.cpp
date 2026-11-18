#include "replora/random_policy.hpp"

namespace replora {

RandomPolicy::RandomPolicy(std::size_t capacity, std::uint32_t seed)
    : CachePolicy(capacity), rng_(seed) {}

AccessResult RandomPolicy::access(Key key) {
  if (index_.find(key) != index_.end()) {
    return AccessResult{true, false, std::nullopt, ReasonCode::Hit};
  }

  if (residents_.size() < capacity_) {
    index_.emplace(key, residents_.size());
    residents_.push_back(key);
    return AccessResult{false, false, std::nullopt, ReasonCode::MissInsertedFree};
  }

  const std::size_t slot = rng_.nextBelow(static_cast<std::uint32_t>(residents_.size()));
  const Key victim = residents_[slot];
  const std::size_t last = residents_.size() - 1;
  if (slot != last) {
    const Key moved = residents_[last];
    residents_[slot] = moved;
    index_[moved] = slot;
  }
  residents_.pop_back();
  index_.erase(victim);

  index_.emplace(key, residents_.size());
  residents_.push_back(key);
  return AccessResult{false, true, victim, ReasonCode::MissEvictedRandomIndex};
}

void RandomPolicy::reset() {
  residents_.clear();
  index_.clear();
  rng_.reset();
}

std::string RandomPolicy::getName() const { return "RANDOM"; }

CacheState RandomPolicy::getCacheState() const {
  CacheState state;
  state.ordering = "arbitrary order (swap-and-pop)";
  state.keys = residents_;
  return state;
}

// Heap estimate, never process RSS. Unused vector capacity is counted; slack buckets are not.
//   sizeof(RandomPolicy)                               object, including the Rng
//   + vector.capacity * sizeof(Key)                    allocated resident storage
//   + residents * (sizeof(Key) + sizeof(size_t) + sizeof(void*))
//                                                      map node: key, index, next
//   + bucket_count * sizeof(void*)                     map bucket array
std::size_t RandomPolicy::estimateMemoryBytes() const {
  const std::size_t residents = residents_.size();
  const std::size_t mapNode = sizeof(Key) + sizeof(std::size_t) + sizeof(void*);
  return sizeof(RandomPolicy) + residents_.capacity() * sizeof(Key) + residents * mapNode +
         index_.bucket_count() * sizeof(void*);
}

}  // namespace replora
