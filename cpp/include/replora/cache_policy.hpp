#pragma once

#include "replora/types.hpp"

#include <cstddef>
#include <string>

namespace replora {

// Shared by the native CLI and the WASM build. One implementation in cpp/src.
// Capacity is fixed for the lifetime of the policy and must be >= 1.
class CachePolicy {
 public:
  // Throws std::invalid_argument when capacity < 1.
  explicit CachePolicy(std::size_t capacity);
  virtual ~CachePolicy();

  CachePolicy(const CachePolicy&) = delete;
  CachePolicy& operator=(const CachePolicy&) = delete;

  // Decision only. Do not build explanation strings here.
  virtual AccessResult access(Key key) = 0;
  virtual void reset() = 0;
  virtual std::string getName() const = 0;
  virtual CacheState getCacheState() const = 0;

  // Extension: estimated overhead from this policy's containers.
  // Each concrete policy documents its formula (sizes and sizeof). Never RSS.
  virtual std::size_t estimateMemoryBytes() const = 0;

  std::size_t capacity() const noexcept;

 protected:
  std::size_t capacity_;
};

}  // namespace replora
