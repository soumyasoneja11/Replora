#include "replora/fifo_policy.hpp"

namespace replora {

FifoPolicy::FifoPolicy(std::size_t capacity) : CachePolicy(capacity) {}

AccessResult FifoPolicy::access(Key key) {
  if (member_.find(key) != member_.end()) {
    return AccessResult{true, false, std::nullopt, ReasonCode::Hit};
  }

  std::optional<Key> victim;
  ReasonCode reason = ReasonCode::MissInsertedFree;
  if (order_.size() >= capacity_) {
    victim = order_.front();
    order_.pop();
    member_.erase(*victim);
    reason = ReasonCode::MissEvictedFifoOldest;
  }

  order_.push(key);
  member_.insert(key);
  return AccessResult{false, victim.has_value(), victim, reason};
}

void FifoPolicy::reset() {
  order_ = std::queue<Key>{};
  member_.clear();
}

std::string FifoPolicy::getName() const { return "FIFO"; }

CacheState FifoPolicy::getCacheState() const {
  CacheState state;
  state.ordering = "front=oldest";
  state.keys.reserve(order_.size());
  std::queue<Key> copy = order_;
  while (!copy.empty()) {
    state.keys.push_back(copy.front());
    copy.pop();
  }
  return state;
}

// Heap estimate, never process RSS. Deque block padding inside the queue is ignored.
//   sizeof(FifoPolicy)                         empty queue and set objects
//   + residents * sizeof(Key)                  one queued key
//   + residents * (sizeof(Key) + sizeof(void*))  set node: key plus next pointer
//   + bucket_count * sizeof(void*)             set bucket array
std::size_t FifoPolicy::estimateMemoryBytes() const {
  const std::size_t residents = order_.size();
  return sizeof(FifoPolicy) + residents * sizeof(Key) +
         residents * (sizeof(Key) + sizeof(void*)) +
         member_.bucket_count() * sizeof(void*);
}

}  // namespace replora
