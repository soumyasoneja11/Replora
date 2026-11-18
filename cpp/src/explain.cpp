#include "replora/engine.hpp"

#include <sstream>

namespace replora {
namespace {

std::string structureClause(const std::string& policyName) {
  if (policyName == "FIFO") {
    return "The \"queue\" keeps oldest at the front. Membership is an \"unordered_set\".";
  }
  if (policyName == "LRU") {
    return "The \"std::list\" tail is the least recently used. The \"unordered_map\" stores an iterator for O(1) promotion.";
  }
  if (policyName == "LFU") {
    return "An \"unordered_map\" holds frequency and insertion order. Eviction scans every resident, which is O(C), with no heap.";
  }
  if (policyName == "RANDOM") {
    return "Residents live in a \"vector\". An \"unordered_map\" stores each key's index for swap-and-pop.";
  }
  return "The policy containers decided the result.";
}

}  // namespace

std::string explainAccess(const std::string& policyName, Key key, const AccessResult& result,
                          const CacheState& state) {
  std::ostringstream text;
  text << policyName << (result.hit ? " hit: " : " miss: ") << "key " << key << ". ";

  switch (result.reason) {
    case ReasonCode::Hit:
      text << "The key was already resident, so nothing was evicted and no victim was chosen. ";
      if (policyName == "FIFO") {
        text << "A FIFO hit does not reorder the queue. ";
      } else if (policyName == "LRU") {
        text << "The node was moved to the front, which is most recently used. ";
      } else if (policyName == "LFU") {
        text << "Its frequency increased by one. Insertion order did not change. ";
      } else if (policyName == "RANDOM") {
        text << "The vector was left unchanged and the RNG was not drawn. ";
      }
      break;
    case ReasonCode::MissInsertedFree:
      text << "The key was absent and the cache still had space, so it was inserted and no victim was chosen. ";
      break;
    case ReasonCode::MissEvictedFifoOldest:
      text << "The key was absent and the queue was full, so the front key ";
      if (result.victim.has_value()) {
        text << *result.victim;
      }
      text << " was evicted because it is the oldest insertion. ";
      break;
    case ReasonCode::MissEvictedLruTail:
      text << "The key was absent and the list was full, so the tail key ";
      if (result.victim.has_value()) {
        text << *result.victim;
      }
      text << " was evicted because it is the least recently used. ";
      break;
    case ReasonCode::MissEvictedLfuMinFreq:
      text << "The key was absent and the cache was full. A full scan chose ";
      if (result.victim.has_value()) {
        text << *result.victim;
      }
      text << " for the lowest frequency, breaking a tie toward the earliest insertion. ";
      break;
    case ReasonCode::MissEvictedRandomIndex:
      text << "The key was absent and the vector was full. A seeded random index selected resident ";
      if (result.victim.has_value()) {
        text << *result.victim;
      }
      text << ", which was removed with swap-and-pop. ";
      break;
  }

  text << structureClause(policyName);
  if (!state.ordering.empty()) {
    text << " Order label: " << state.ordering << ".";
  }
  return text.str();
}

}  // namespace replora
