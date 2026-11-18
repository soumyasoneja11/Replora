#include "golden_fixture.hpp"
#include "replora_cli_path.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::string readFile(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  std::ostringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}

std::string stripCarriage(std::string text) {
  text.erase(std::remove(text.begin(), text.end(), '\r'), text.end());
  return text;
}

struct CliRun {
  int code = -1;
  std::string out;
  std::string err;
};

CliRun runCli(const std::string& args) {
  static int sequence = 0;
  const auto directory = std::filesystem::temp_directory_path();
  const std::string stamp = std::to_string(++sequence) + "_" +
#ifdef _WIN32
                            std::to_string(_getpid()) + "_" +
#else
                            std::to_string(getpid()) + "_" +
#endif
                            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
  const auto outPath = directory / ("replora_cli_" + stamp + ".out");
  const auto errPath = directory / ("replora_cli_" + stamp + ".err");
  // system() invokes cmd /c. A leading quote is stripped together with the
  // final quote, so the command is wrapped in an extra pair.
  const std::string command = std::string("\"\"") + kReploraCliPath + "\" " + args + " > \"" +
                              outPath.string() + "\" 2> \"" + errPath.string() + "\"\"";
  const int status = std::system(command.c_str());
  CliRun run;
  run.code = status;
  run.out = readFile(outPath);
  run.err = readFile(errPath);
  std::filesystem::remove(outPath);
  std::filesystem::remove(errPath);
  return run;
}

std::string stripTiming(std::string json) {
  const char* keys[] = {"executionTimeMicros", "executionTimeMs"};
  for (const char* key : keys) {
    const std::string needle = std::string("\"") + key + "\": ";
    std::size_t position = 0;
    while ((position = json.find(needle, position)) != std::string::npos) {
      const std::size_t value = position + needle.size();
      std::size_t end = value;
      while (end < json.size() && json[end] != ',' && json[end] != '\n') {
        ++end;
      }
      json.replace(value, end - value, "0");
      position = value + 1;
    }
  }
  return json;
}

const replora::test::Json& requireField(const replora::test::Json& node, const char* key) {
  const replora::test::Json* child = node.find(key);
  if (child == nullptr) {
    throw std::runtime_error(std::string("missing ") + key);
  }
  return *child;
}

const replora::test::Json* findPolicy(const replora::test::Json& results, const std::string& name) {
  for (const replora::test::Json& policy : results.array) {
    if (requireField(policy, "policyName").text == name) {
      return &policy;
    }
  }
  return nullptr;
}

void expectGolden(const std::string& fileName, const std::string& args) {
  const CliRun run = runCli(args + " --format json --events");
  ASSERT_EQ(run.code, 0) << run.err << "\n" << run.out;
  ASSERT_TRUE(run.err.empty()) << run.err;

  const replora::test::Json root = replora::test::parseJson(run.out);
  const replora::test::Json& trace = requireField(root, "trace");
  const replora::test::Json& results = requireField(root, "results");
  const std::string path = std::string(REPLORA_GOLDEN_DIR) + "/" + fileName;

  const char* policies[] = {"FIFO", "LRU", "LFU"};
  for (const char* policyName : policies) {
    const replora::test::GoldenPolicyRun golden =
        replora::test::loadGoldenPolicy(path, policyName);
    const replora::test::Json* policy = findPolicy(results, policyName);
    ASSERT_NE(policy, nullptr) << policyName;
    const replora::test::Json& metrics = requireField(*policy, "metrics");
    EXPECT_EQ(static_cast<std::uint64_t>(requireField(metrics, "hits").number), golden.hits);
    EXPECT_EQ(static_cast<std::uint64_t>(requireField(metrics, "misses").number), golden.misses);
    EXPECT_EQ(static_cast<std::uint64_t>(requireField(metrics, "evictions").number),
              golden.evictions);

    ASSERT_EQ(trace.array.size(), golden.trace.size());
    for (std::size_t i = 0; i < golden.trace.size(); ++i) {
      EXPECT_EQ(static_cast<replora::Key>(trace.array[i].number), golden.trace[i]);
    }

    const replora::test::Json& events = requireField(*policy, "events");
    ASSERT_EQ(events.array.size(), golden.steps.size()) << policyName;
    for (std::size_t i = 0; i < golden.steps.size(); ++i) {
      const replora::test::Json& event = events.array[i];
      const replora::test::GoldenStep& step = golden.steps[i];
      EXPECT_EQ(requireField(event, "hit").boolean, step.hit) << policyName << " " << i;
      EXPECT_EQ(requireField(event, "evicted").boolean, step.evicted) << policyName << " " << i;
      const replora::test::Json& victim = requireField(event, "victim");
      if (step.victim.has_value()) {
        EXPECT_EQ(victim.kind, replora::test::Json::Kind::Number);
        EXPECT_EQ(static_cast<replora::Key>(victim.number), *step.victim);
      } else {
        EXPECT_EQ(victim.kind, replora::test::Json::Kind::Null);
      }
      std::vector<replora::Key> expected = step.cacheStateAfter;
      if (std::string(policyName) == "LRU") {
        std::reverse(expected.begin(), expected.end());
      }
      const replora::test::Json& keys = requireField(requireField(event, "stateAfter"), "keys");
      ASSERT_EQ(keys.array.size(), expected.size());
      for (std::size_t k = 0; k < expected.size(); ++k) {
        EXPECT_EQ(static_cast<replora::Key>(keys.array[k].number), expected[k]);
      }
    }
  }
}

}  // namespace

TEST(Cli, JsonListsFourPolicies) {
  const CliRun run =
      runCli("--capacity 4 --workload looping --requests 8 --format json");
  ASSERT_EQ(run.code, 0) << run.err;
  const replora::test::Json root = replora::test::parseJson(run.out);
  const replora::test::Json& results = requireField(root, "results");
  ASSERT_EQ(results.array.size(), 4u);
  const char* names[] = {"FIFO", "LRU", "LFU", "RANDOM"};
  for (std::size_t i = 0; i < 4; ++i) {
    EXPECT_EQ(requireField(results.array[i], "policyName").text, names[i]);
    EXPECT_TRUE(requireField(results.array[i], "events").array.empty());
  }
  EXPECT_EQ(requireField(requireField(root, "config"), "seed").number, 42);
  EXPECT_TRUE(requireField(root, "errors").array.empty());
}

TEST(Cli, OnePolicyReturnsOnlyLru) {
  const CliRun run =
      runCli("--capacity 3 --workload sequential --requests 6 --policy lru --format json");
  ASSERT_EQ(run.code, 0) << run.err;
  const replora::test::Json root = replora::test::parseJson(run.out);
  const replora::test::Json& results = requireField(root, "results");
  ASSERT_EQ(results.array.size(), 1u);
  EXPECT_EQ(requireField(results.array[0], "policyName").text, "LRU");
}

TEST(Cli, CsvHasHeaderAndFourRows) {
  const CliRun run =
      runCli("--capacity 4 --workload sequential --requests 5 --seed 1 --format csv");
  ASSERT_EQ(run.code, 0) << run.err;
  const std::string text = stripCarriage(run.out);
  std::vector<std::string> rows;
  std::string row;
  std::istringstream lines(text);
  while (std::getline(lines, row)) {
    if (!row.empty()) {
      rows.push_back(row);
    }
  }
  ASSERT_EQ(rows.size(), 5u);
  EXPECT_EQ(rows[0],
            "policy,workload,capacity,requests,seed,hits,misses,evictions,"
            "hitRate,missRate,executionTimeMicros,estimatedMemoryBytes");
  EXPECT_EQ(rows[1].rfind("FIFO,", 0), 0u);
  EXPECT_EQ(rows[2].rfind("LRU,", 0), 0u);
  EXPECT_EQ(rows[3].rfind("LFU,", 0), 0u);
  EXPECT_EQ(rows[4].rfind("RANDOM,", 0), 0u);
}

TEST(Cli, InvalidUsageExits2) {
  const char* args[] = {
      "",
      "--workload looping --requests 1",
      "--capacity 4 --requests 1",
      "--capacity 4 --workload looping",
      "--capacity 0 --workload looping --requests 1",
      "--capacity -3 --workload looping --requests 1",
      "--capacity 4 --workload looping --requests -1",
      "--capacity 4 --workload nope --requests 1",
      "--capacity 4 --workload looping --requests 1 --policy mru",
      "--capacity 4 --workload looping --requests 1 --seed -2",
      "--capacity 4 --workload looping --requests 1 --format xml",
  };
  for (const char* arg : args) {
    const CliRun run = runCli(arg);
    EXPECT_EQ(run.code, 2) << arg << "\nstdout:\n" << run.out << "\nstderr:\n" << run.err;
    EXPECT_FALSE(run.err.empty()) << arg;
  }
}

TEST(Cli, HelpExits0) {
  const CliRun run = runCli("--help");
  EXPECT_EQ(run.code, 0) << run.err;
  EXPECT_NE(run.out.find("Usage:"), std::string::npos);
}

TEST(Cli, SameFlagsProduceTheSameJsonExceptTiming) {
  const std::string args =
      "--capacity 4 --workload bursty --requests 30 --seed 7 --policy all --format json --events";
  const CliRun first = runCli(args);
  const CliRun second = runCli(args);
  ASSERT_EQ(first.code, 0) << first.err;
  ASSERT_EQ(second.code, 0) << second.err;
  EXPECT_EQ(stripTiming(first.out), stripTiming(second.out));
}

TEST(Cli, JsonMatchesTwoGoldenConfigsForFifoLruLfu) {
  expectGolden("looping_c4_n20_s42.json",
               "--capacity 4 --workload looping --requests 20 --seed 42");
  expectGolden("bursty_c2_n20_s7.json",
               "--capacity 2 --workload bursty --requests 20 --seed 7");
}
