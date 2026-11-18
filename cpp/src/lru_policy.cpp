#include "replora/lru_policy.hpp"

namespace replora {

LruPolicy::LruPolicy(std::size_t capacity) : CachePolicy(capacity) {}

AccessResult LruPolicy::access(Key key) {
  // O(1) average hash lookup.
  const auto found = index_.find(key);
  if (found != index_.end()) {
    // O(1) promote to MRU. Skip splice when the node is already front:
    // splice(pos, *this, it) is undefined if it == pos.
    if (found->second != order_.begin()) {
      order_.splice(order_.begin(), order_, found->second);
    }
    return AccessResult{true, false, std::nullopt, ReasonCode::Hit};
  }

  std::optional<Key> victim;
  ReasonCode reason = ReasonCode::MissInsertedFree;
  if (order_.size() >= capacity_) {
    // O(1) evict the LRU at the back, then O(1) average map erase.
    victim = order_.back();
    index_.erase(*victim);
    order_.pop_back();
    reason = ReasonCode::MissEvictedLruTail;
  }

  // O(1) insert as the new MRU, then O(1) average map insert.
  order_.push_front(key);
  index_.emplace(key, order_.begin());
  return AccessResult{false, victim.has_value(), victim, reason};
}

void LruPolicy::reset() {
  // O(n) to destroy every node and map entry.
  order_.clear();
  index_.clear();
}

std::string LruPolicy::getName() const { return "LRU"; }

CacheState LruPolicy::getCacheState() const {
  // O(n) copy. Front of the list is MRU.
  CacheState state;
  state.ordering = "front=MRU, back=LRU";
  state.keys.reserve(order_.size());
  for (const Key key : order_) {
    state.keys.push_back(key);
  }
  return state;
}

// Heap estimate, never process RSS. Allocator padding is ignored.
//   sizeof(LruPolicy)                                      empty list and map
//   + residents * (sizeof(Key) + 2 * sizeof(void*))        list node: key, prev, next
//   + residents * (sizeof(Key) + sizeof(list::iterator) + sizeof(void*))
//                                                          map node: key, iterator, next
//   + bucket_count * sizeof(void*)                         map bucket array
std::size_t LruPolicy::estimateMemoryBytes() const {
  const std::size_t residents = order_.size();
  const std::size_t listNode = sizeof(Key) + 2 * sizeof(void*);
  const std::size_t mapNode = sizeof(Key) + sizeof(std::list<Key>::iterator) + sizeof(void*);
  return sizeof(LruPolicy) + residents * listNode + residents * mapNode +
         index_.bucket_count() * sizeof(void*);
}

}  // namespace replora
