#include "replora/json.hpp"

#include <cstdio>
#include <string>
#include <string_view>

namespace replora {
namespace {

void appendEscaped(std::string& out, std::string_view text) {
  out.push_back('"');
  for (const unsigned char character : text) {
    switch (character) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\b':
        out += "\\b";
        break;
      case '\f':
        out += "\\f";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        if (character < 0x20) {
          char buffer[8];
          std::snprintf(buffer, sizeof(buffer), "\\u%04x", character);
          out += buffer;
        } else {
          out.push_back(static_cast<char>(character));
        }
        break;
    }
  }
  out.push_back('"');
}

void appendNumber(std::string& out, double value) {
  char buffer[64];
  std::snprintf(buffer, sizeof(buffer), "%.17g", value);
  out += buffer;
}

const char* selectionName(PolicySelection selection) {
  return selection == PolicySelection::One ? "one" : "all";
}

}  // namespace

std::string toJson(const SimulationResult& result) {
  std::string out;
  out += "{\n";
  out += "  \"config\": {\n";
  out += "    \"workload\": ";
  appendEscaped(out, toString(result.config.workload));
  out += ",\n    \"capacity\": ";
  out += std::to_string(result.config.capacity);
  out += ",\n    \"traceLength\": ";
  out += std::to_string(result.config.traceLength);
  out += ",\n    \"seed\": ";
  out += std::to_string(result.config.seed);
  out += ",\n    \"policySelection\": ";
  appendEscaped(out, selectionName(result.config.policySelection));
  out += ",\n    \"policyName\": ";
  appendEscaped(out, result.config.policyName);
  out += ",\n    \"workloadName\": ";
  appendEscaped(out, result.config.workloadName);
  out += "\n  },\n  \"trace\": [";
  for (std::size_t i = 0; i < result.trace.size(); ++i) {
    if (i != 0) {
      out += ", ";
    }
    out += std::to_string(result.trace[i]);
  }
  out += "],\n  \"results\": [";
  for (std::size_t p = 0; p < result.results.size(); ++p) {
    const PolicyResult& policy = result.results[p];
    if (p != 0) {
      out += ", ";
    }
    out += "{\n    \"policyName\": ";
    appendEscaped(out, policy.policyName);
    out += ",\n    \"metrics\": {\n";
    out += "      \"hits\": " + std::to_string(policy.metrics.hits);
    out += ",\n      \"misses\": " + std::to_string(policy.metrics.misses);
    out += ",\n      \"evictions\": " + std::to_string(policy.metrics.evictions);
    out += ",\n      \"hitRate\": ";
    appendNumber(out, policy.metrics.hitRate);
    out += ",\n      \"missRate\": ";
    appendNumber(out, policy.metrics.missRate);
    out += ",\n      \"executionTimeMs\": ";
    appendNumber(out, policy.metrics.executionTimeMs);
    out += ",\n      \"executionTimeMicros\": " + std::to_string(policy.metrics.executionTimeMicros);
    out += ",\n      \"totalAccesses\": " + std::to_string(policy.metrics.totalAccesses);
    out += ",\n      \"cacheCapacity\": " + std::to_string(policy.metrics.cacheCapacity);
    out += ",\n      \"estimatedMemoryBytes\": " + std::to_string(policy.metrics.estimatedMemoryBytes);
    out += "\n    },\n    \"events\": [";
    for (std::size_t e = 0; e < policy.events.size(); ++e) {
      const AccessEvent& event = policy.events[e];
      if (e != 0) {
        out += ", ";
      }
      out += "{\n      \"index\": " + std::to_string(event.index);
      out += ",\n      \"key\": " + std::to_string(event.key);
      out += ",\n      \"hit\": ";
      out += event.hit ? "true" : "false";
      out += ",\n      \"evicted\": ";
      out += event.evicted ? "true" : "false";
      out += ",\n      \"victim\": ";
      if (event.victim.has_value()) {
        out += std::to_string(*event.victim);
      } else {
        out += "null";
      }
      out += ",\n      \"stateAfter\": {\n        \"keys\": [";
      for (std::size_t k = 0; k < event.stateAfter.keys.size(); ++k) {
        if (k != 0) {
          out += ", ";
        }
        out += std::to_string(event.stateAfter.keys[k]);
      }
      out += "],\n        \"frequencies\": [";
      for (std::size_t k = 0; k < event.stateAfter.frequencies.size(); ++k) {
        if (k != 0) {
          out += ", ";
        }
        out += std::to_string(event.stateAfter.frequencies[k]);
      }
      out += "],\n        \"ordering\": ";
      appendEscaped(out, event.stateAfter.ordering);
      out += "\n      },\n      \"policyName\": ";
      appendEscaped(out, event.policyName);
      out += ",\n      \"explanation\": ";
      appendEscaped(out, event.explanation);
      out += "\n    }";
    }
    out += "]\n  }";
  }
  out += "],\n  \"errors\": [";
  for (std::size_t i = 0; i < result.errors.size(); ++i) {
    if (i != 0) {
      out += ", ";
    }
    out += "{\n    \"field\": ";
    appendEscaped(out, result.errors[i].field);
    out += ",\n    \"message\": ";
    appendEscaped(out, result.errors[i].message);
    out += "\n  }";
  }
  out += "]\n}\n";
  return out;
}

}  // namespace replora
