#include "replora/engine.hpp"
#include "replora/json.hpp"

#include <charconv>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace {

constexpr const char* kUsage =
    "Usage: replora_cli --capacity N --workload NAME --requests N [options]\n"
    "\n"
    "Required:\n"
    "  --capacity N     Cache slots. Must be >= 1.\n"
    "  --workload NAME  sequential, random, looping, bursty, or skewed.\n"
    "                   frequency-skewed is the same workload as skewed.\n"
    "  --requests N     Trace length. 0 through 10000.\n"
    "\n"
    "Options:\n"
    "  --seed N         Generator seed. Default 42.\n"
    "  --policy NAME    fifo, lru, lfu, random, or all. Default all.\n"
    "  --format NAME    table, json, or csv. Default table.\n"
    "  --events         Include per-access events in json output.\n"
    "  --help           Show this help and exit 0.\n"
    "\n"
    "Exit 0 on success. Exit 2 on invalid usage.\n"
    "\n"
    "csv columns, in order, for a later results/benchmark_results.csv:\n"
    "  policy,workload,capacity,requests,seed,hits,misses,evictions,\n"
    "  hitRate,missRate,executionTimeMicros,estimatedMemoryBytes\n";

int fail(std::string_view message) {
  std::cerr << "replora_cli: " << message << "\n";
  std::cerr << "Try 'replora_cli --help' for usage.\n";
  return 2;
}

std::optional<std::int64_t> parseInteger(std::string_view text) {
  if (text.empty()) {
    return std::nullopt;
  }
  std::int64_t value = 0;
  const char* const begin = text.data();
  const char* const end = text.data() + text.size();
  const std::from_chars_result parsed = std::from_chars(begin, end, value);
  if (parsed.ec != std::errc{} || parsed.ptr != end) {
    return std::nullopt;
  }
  return value;
}

bool takeValue(int argc, char** argv, int& index, std::string_view& value) {
  if (index + 1 >= argc) {
    return false;
  }
  value = argv[++index];
  if (value.empty() || (value.size() >= 2 && value[0] == '-' && value[1] == '-')) {
    return false;
  }
  return true;
}

std::optional<replora::WorkloadType> workloadFromCli(std::string_view name) {
  if (name == "skewed") {
    return replora::WorkloadType::FrequencySkewed;
  }
  return replora::workloadFromString(name);
}

bool policyFromCli(std::string_view name, replora::SimulationConfig& config) {
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
    return false;
  }
  return true;
}

void appendNumber(std::string& out, double value) {
  char buffer[64];
  std::snprintf(buffer, sizeof(buffer), "%.17g", value);
  out += buffer;
}

// Same columns as the help text. One header row, then one row per policy.
std::string toCsv(const replora::SimulationResult& result) {
  std::string out =
      "policy,workload,capacity,requests,seed,hits,misses,evictions,"
      "hitRate,missRate,executionTimeMicros,estimatedMemoryBytes\n";
  const char* workload = replora::toString(result.config.workload);
  for (const replora::PolicyResult& policy : result.results) {
    out += policy.policyName;
    out += ',';
    out += workload;
    out += ',';
    out += std::to_string(result.config.capacity);
    out += ',';
    out += std::to_string(result.config.traceLength);
    out += ',';
    out += std::to_string(result.config.seed);
    out += ',';
    out += std::to_string(policy.metrics.hits);
    out += ',';
    out += std::to_string(policy.metrics.misses);
    out += ',';
    out += std::to_string(policy.metrics.evictions);
    out += ',';
    appendNumber(out, policy.metrics.hitRate);
    out += ',';
    appendNumber(out, policy.metrics.missRate);
    out += ',';
    out += std::to_string(policy.metrics.executionTimeMicros);
    out += ',';
    out += std::to_string(policy.metrics.estimatedMemoryBytes);
    out += '\n';
  }
  return out;
}

void printTable(const replora::SimulationResult& result) {
  std::cout << "workload: " << replora::toString(result.config.workload) << "\n";
  std::cout << "capacity: " << result.config.capacity << "\n";
  std::cout << "requests: " << result.config.traceLength << "\n";
  std::cout << "seed: " << result.config.seed << "\n";
  for (const replora::PolicyResult& policy : result.results) {
    const replora::PolicyMetrics& metrics = policy.metrics;
    std::cout << "\n" << policy.policyName << "\n";
    std::cout << "  hits: " << metrics.hits << "\n";
    std::cout << "  misses: " << metrics.misses << "\n";
    std::cout << "  evictions: " << metrics.evictions << "\n";
    std::cout << "  hitRate: " << metrics.hitRate << "\n";
    std::cout << "  missRate: " << metrics.missRate << "\n";
    std::cout << "  executionTimeMicros: " << metrics.executionTimeMicros << "\n";
    std::cout << "  estimatedMemoryBytes: " << metrics.estimatedMemoryBytes << "\n";
  }
}

}  // namespace

int main(int argc, char** argv) {
  bool help = false;
  bool events = false;
  bool sawCapacity = false;
  bool sawWorkload = false;
  bool sawRequests = false;
  std::int64_t capacity = 0;
  std::int64_t requests = 0;
  std::uint32_t seed = 42;
  std::string workloadName;
  std::string policyName = "all";
  std::string format = "table";

  for (int index = 1; index < argc; ++index) {
    const std::string_view arg = argv[index];
    if (arg == "--help") {
      help = true;
      continue;
    }
    if (arg == "--events") {
      events = true;
      continue;
    }

    std::string_view value;
    if (arg == "--capacity" || arg == "--workload" || arg == "--requests" || arg == "--seed" ||
        arg == "--policy" || arg == "--format") {
      if (!takeValue(argc, argv, index, value)) {
        return fail(std::string(arg) + " needs a value");
      }
    } else {
      return fail(std::string("unknown argument ") + argv[index]);
    }

    if (arg == "--capacity") {
      const std::optional<std::int64_t> parsed = parseInteger(value);
      if (!parsed.has_value()) {
        return fail("--capacity expects an integer");
      }
      capacity = *parsed;
      sawCapacity = true;
    } else if (arg == "--requests") {
      const std::optional<std::int64_t> parsed = parseInteger(value);
      if (!parsed.has_value()) {
        return fail("--requests expects an integer");
      }
      requests = *parsed;
      sawRequests = true;
    } else if (arg == "--seed") {
      const std::optional<std::int64_t> parsed = parseInteger(value);
      if (!parsed.has_value() || *parsed < 0 ||
          static_cast<std::uint64_t>(*parsed) > static_cast<std::uint64_t>(UINT32_MAX)) {
        return fail("--seed expects an integer from 0 through 4294967295");
      }
      seed = static_cast<std::uint32_t>(*parsed);
    } else if (arg == "--workload") {
      workloadName = std::string(value);
      sawWorkload = true;
    } else if (arg == "--policy") {
      policyName = std::string(value);
    } else if (arg == "--format") {
      format = std::string(value);
    }
  }

  if (help) {
    std::cout << kUsage;
    return 0;
  }
  if (!sawCapacity) {
    return fail("missing required flag --capacity");
  }
  if (!sawWorkload) {
    return fail("missing required flag --workload");
  }
  if (!sawRequests) {
    return fail("missing required flag --requests");
  }
  if (capacity < 1) {
    return fail("capacity must be >= 1");
  }
  if (requests < 0) {
    return fail("requests must be >= 0");
  }
  if (requests > replora::kMaxTraceLength) {
    return fail("requests must be <= 10000");
  }
  if (format != "table" && format != "json" && format != "csv") {
    return fail("unknown format '" + format + "'");
  }

  replora::SimulationConfig config;
  config.capacity = capacity;
  config.traceLength = requests;
  config.seed = seed;
  const std::optional<replora::WorkloadType> workload = workloadFromCli(workloadName);
  if (!workload.has_value()) {
    return fail("unknown workload '" + workloadName + "'");
  }
  config.workload = *workload;
  if (!policyFromCli(policyName, config)) {
    return fail("unknown policy '" + policyName + "'");
  }

  const replora::SimulationResult result = replora::runSimulation(config);
  if (!result.errors.empty()) {
    for (const replora::ConfigError& error : result.errors) {
      std::cerr << "replora_cli: " << error.field << ": " << error.message << "\n";
    }
    return 2;
  }

  if (format == "json") {
    replora::SimulationResult printed = result;
    if (!events) {
      for (replora::PolicyResult& policy : printed.results) {
        policy.events.clear();
      }
    }
    std::cout << replora::toJson(printed);
    return 0;
  }
  if (format == "csv") {
    std::cout << toCsv(result);
    return 0;
  }
  printTable(result);
  return 0;
}
