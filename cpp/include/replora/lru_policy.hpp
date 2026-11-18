#pragma once

#include "replora/cache_policy.hpp"

#include <list>
#include <unordered_map>

namespace replora {

// LRU: std::list front is MRU, back is LRU, plus a map from key to list iterator.
//
// list::splice relinks an existing node. It does not allocate or destroy that
// node, and it does not invalidate iterators or references to list elements.
// The map stores those iterators, so a hit can move a node to the front
// without updating the map. The only iterator that dies is the victim's, and
// that map entry is erased in the same step as pop_back.
class LruPolicy final : public CachePolicy {
 public:
  explicit LruPolicy(std::size_t capacity);

  AccessResult access(Key key) override;
  void reset() override;
  std::string getName() const override;
  CacheState getCacheState() const override;
  std::size_t estimateMemoryBytes() const override;

  // Test only. Both sizes must stay equal after every access.
  std::size_t debugListSize() const { return order_.size(); }
  std::size_t debugMapSize() const { return index_.size(); }

 private:
  std::list<Key> order_;
  std::unordered_map<Key, std::list<Key>::iterator> index_;
};

}  // namespace replora
