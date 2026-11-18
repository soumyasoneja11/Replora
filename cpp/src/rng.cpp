#include "replora/rng.hpp"

namespace replora {
namespace {

constexpr std::uint32_t kMultiplier = 1664525u;
constexpr std::uint32_t kIncrement = 1013904223u;
constexpr double kModulus = 4294967296.0;

}  // namespace

Rng::Rng(std::uint32_t seed) : seed_(seed), state_(seed) {}

double Rng::nextUnit() {
  state_ = state_ * kMultiplier + kIncrement;
  return static_cast<double>(state_) / kModulus;
}

std::uint32_t Rng::nextBelow(std::uint32_t exclusiveUpper) {
  return static_cast<std::uint32_t>(nextUnit() * static_cast<double>(exclusiveUpper));
}

void Rng::reset() { state_ = seed_; }

}  // namespace replora
