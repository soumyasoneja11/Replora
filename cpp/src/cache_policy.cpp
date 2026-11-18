#include "replora/cache_policy.hpp"

#include <stdexcept>

namespace replora {

CachePolicy::CachePolicy(std::size_t capacity) : capacity_(capacity) {
  if (capacity < 1) {
    throw std::invalid_argument("capacity must be >= 1");
  }
}

CachePolicy::~CachePolicy() = default;

std::size_t CachePolicy::capacity() const noexcept { return capacity_; }

}  // namespace replora
