#include "replora/engine.hpp"
#include "replora/json.hpp"

#include <emscripten/bind.h>

#include <cstdint>
#include <cstdio>
#include <exception>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {

std::string escapeJson(std::string_view text) {
  std::string out;
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
  return out;
}

std::string errorJson(const std::vector<replora::ConfigError>& errors) {
  std::string out = "{\"error\":{\"field\":";
  out += escapeJson(errors.empty() ? "exception" : errors.front().field);
  out += ",\"message\":";
  out += escapeJson(errors.empty() ? "unknown error" : errors.front().message);
  out += ",\"errors\":[";
  for (std::size_t i = 0; i < errors.size(); ++i) {
    if (i != 0) {
      out += ',';
    }
    out += "{\"field\":";
    out += escapeJson(errors[i].field);
    out += ",\"message\":";
    out += escapeJson(errors[i].message);
    out += '}';
  }
  out += "]}}";
  return out;
}

std::string errorJson(std::string_view field, std::string_view message) {
  return errorJson(std::vector<replora::ConfigError>{replora::ConfigError{std::string(field), std::string(message)}});
}

std::optional<replora::WorkloadType> workloadFromBoundary(std::string_view name) {
  if (name == "skewed") {
    return replora::WorkloadType::FrequencySkewed;
  }
  return replora::workloadFromString(name);
}

bool policyFromBoundary(std::string_view name, replora::SimulationConfig& config) {
  if (name == "all") {
    config.policySelection = replora::PolicySelection::All;
    config.policyName.clear();
    return true;
  }
  config.policySelection = replora::PolicySelection::One;
  if (name == "fifo") {
    config.policyName = "FIFO";
  } else if (name == "lru") {
    config.policyName = "LRU";
  } else if (name == "lfu") {
    config.policyName = "LFU";
  } else if (name == "random") {
    config.policyName = "RANDOM";
  } else {
    config.policyName = std::string(name);
    return false;
  }
  return true;
}

std::string runSimulationJson(int capacity, std::string workload, int length, int seed,
                              std::string policy, bool includeEvents) {
  try {
    if (seed < 0) {
      return errorJson("seed", "seed must be >= 0");
    }

    replora::SimulationConfig config;
    config.capacity = capacity;
    config.traceLength = length;
    config.seed = static_cast<std::uint32_t>(seed);
    if (const std::optional<replora::WorkloadType> parsed = workloadFromBoundary(workload)) {
      config.workload = *parsed;
    } else {
      config.workloadName = std::move(workload);
    }
    policyFromBoundary(policy, config);

    replora::SimulationResult result = replora::runSimulation(config);
    if (!result.errors.empty()) {
      return errorJson(result.errors);
    }
    if (!includeEvents) {
      for (replora::PolicyResult& policyResult : result.results) {
        policyResult.events.clear();
      }
    }
    return replora::toJson(result);
  } catch (const std::exception& error) {
    return errorJson("exception", error.what());
  } catch (...) {
    return errorJson("exception", "unknown error");
  }
}

}  // namespace

EMSCRIPTEN_BINDINGS(replora_wasm) {
  emscripten::function("runSimulation", &runSimulationJson);
}
