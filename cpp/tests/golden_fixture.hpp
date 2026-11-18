#pragma once

// Shared golden loader for policy tests. Pass the policy name ("FIFO", "LRU", "LFU", "RANDOM").

#include "replora/types.hpp"

#include <charconv>
#include <cstdint>
#include <fstream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace replora::test {

struct Json {
  enum class Kind { Null, Bool, Number, String, Array, Object };

  Kind kind = Kind::Null;
  bool boolean = false;
  std::int64_t number = 0;
  std::string text;
  std::vector<Json> array;
  std::vector<std::pair<std::string, Json>> object;

  const Json* find(std::string_view key) const {
    if (kind != Kind::Object) {
      return nullptr;
    }
    for (const auto& entry : object) {
      if (entry.first == key) {
        return &entry.second;
      }
    }
    return nullptr;
  }
};

struct GoldenStep {
  bool hit = false;
  bool evicted = false;
  std::optional<Key> victim;
  std::vector<Key> cacheStateAfter;
};

struct GoldenPolicyRun {
  std::string workload;
  std::uint64_t capacity = 0;
  std::uint64_t traceLength = 0;
  std::uint32_t seed = 0;
  std::vector<Key> trace;
  std::vector<GoldenStep> steps;
  std::uint64_t hits = 0;
  std::uint64_t misses = 0;
  std::uint64_t evictions = 0;
};

inline Json parseJson(std::string_view input);

inline GoldenPolicyRun loadGoldenPolicy(const std::string& path, const std::string& policyName) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    throw std::runtime_error("could not open golden file: " + path);
  }
  std::ostringstream buffer;
  buffer << file.rdbuf();
  const Json root = parseJson(buffer.str());

  const auto require = [](const Json& node, const char* key) -> const Json& {
    const Json* child = node.find(key);
    if (child == nullptr) {
      throw std::runtime_error(std::string("golden file missing ") + key);
    }
    return *child;
  };

  const Json& config = require(root, "config");
  const Json& trace = require(root, "trace");
  const Json& policies = require(root, "policies");
  const Json* policy = policies.find(policyName);
  if (policy == nullptr) {
    throw std::runtime_error("golden file missing policy " + policyName);
  }
  const Json& accesses = require(*policy, "accesses");

  GoldenPolicyRun run;
  run.workload = require(config, "workload").text;
  run.capacity = static_cast<std::uint64_t>(require(config, "capacity").number);
  run.traceLength = static_cast<std::uint64_t>(require(config, "traceLength").number);
  run.seed = static_cast<std::uint32_t>(require(config, "seed").number);
  run.hits = static_cast<std::uint64_t>(require(*policy, "hits").number);
  run.misses = static_cast<std::uint64_t>(require(*policy, "misses").number);
  run.evictions = static_cast<std::uint64_t>(require(*policy, "evictions").number);

  if (trace.kind != Json::Kind::Array || accesses.kind != Json::Kind::Array) {
    throw std::runtime_error("golden trace or accesses is not an array");
  }

  run.trace.reserve(trace.array.size());
  for (const Json& key : trace.array) {
    run.trace.push_back(static_cast<Key>(key.number));
  }

  run.steps.reserve(accesses.array.size());
  for (const Json& access : accesses.array) {
    GoldenStep step;
    step.hit = require(access, "hit").boolean;
    step.evicted = require(access, "evicted").boolean;
    const Json& victim = require(access, "victim");
    if (victim.kind == Json::Kind::Number) {
      step.victim = static_cast<Key>(victim.number);
    } else if (victim.kind != Json::Kind::Null) {
      throw std::runtime_error("golden victim is not null or a number");
    }
    const Json& state = require(access, "cacheStateAfter");
    if (state.kind != Json::Kind::Array) {
      throw std::runtime_error("golden cacheStateAfter is not an array");
    }
    step.cacheStateAfter.reserve(state.array.size());
    for (const Json& key : state.array) {
      step.cacheStateAfter.push_back(static_cast<Key>(key.number));
    }
    run.steps.push_back(std::move(step));
  }

  return run;
}

inline Json parseJson(std::string_view input) {
  struct Parser {
    std::string_view in;
    std::size_t index = 0;

    [[noreturn]] void fail(const char* message) const {
      throw std::runtime_error(std::string(message) + " at " + std::to_string(index));
    }

    void skip() {
      while (index < in.size() &&
             (in[index] == ' ' || in[index] == '\n' || in[index] == '\r' || in[index] == '\t')) {
        ++index;
      }
    }

    char peek() {
      skip();
      if (index >= in.size()) {
        fail("unexpected end of json");
      }
      return in[index];
    }

    char get() {
      const char character = peek();
      ++index;
      return character;
    }

    void expect(char character) {
      if (get() != character) {
        fail("unexpected character");
      }
    }

    Json parseValue() {
      const char character = peek();
      if (character == '{') {
        return parseObject();
      }
      if (character == '[') {
        return parseArray();
      }
      if (character == '"') {
        return parseString();
      }
      if (character == 't' || character == 'f') {
        return parseBool();
      }
      if (character == 'n') {
        return parseNull();
      }
      return parseNumber();
    }

    Json parseObject() {
      expect('{');
      Json value;
      value.kind = Json::Kind::Object;
      skip();
      if (peek() == '}') {
        get();
        return value;
      }
      while (true) {
        Json key = parseString();
        expect(':');
        value.object.emplace_back(std::move(key.text), parseValue());
        skip();
        if (peek() == '}') {
          get();
          break;
        }
        expect(',');
      }
      return value;
    }

    Json parseArray() {
      expect('[');
      Json value;
      value.kind = Json::Kind::Array;
      skip();
      if (peek() == ']') {
        get();
        return value;
      }
      while (true) {
        value.array.push_back(parseValue());
        skip();
        if (peek() == ']') {
          get();
          break;
        }
        expect(',');
      }
      return value;
    }

    Json parseString() {
      expect('"');
      Json value;
      value.kind = Json::Kind::String;
      while (index < in.size() && in[index] != '"') {
        if (in[index] == '\\') {
          ++index;
          if (index >= in.size()) {
            fail("bad string escape");
          }
        }
        value.text.push_back(in[index]);
        ++index;
      }
      expect('"');
      return value;
    }

    Json parseBool() {
      Json value;
      value.kind = Json::Kind::Bool;
      if (in.substr(index, 4) == "true") {
        value.boolean = true;
        index += 4;
        return value;
      }
      if (in.substr(index, 5) == "false") {
        index += 5;
        return value;
      }
      fail("invalid boolean");
    }

    Json parseNull() {
      if (in.substr(index, 4) != "null") {
        fail("invalid null");
      }
      index += 4;
      Json value;
      value.kind = Json::Kind::Null;
      return value;
    }

    Json parseNumber() {
      const char* first = in.data() + index;
      const char* last = in.data() + in.size();
      std::int64_t number = 0;
      const std::from_chars_result parsed = std::from_chars(first, last, number);
      if (parsed.ec != std::errc{}) {
        fail("invalid number");
      }
      index = static_cast<std::size_t>(parsed.ptr - in.data());
      Json value;
      value.kind = Json::Kind::Number;
      value.number = number;
      return value;
    }
  };

  Parser parser{input, 0};
  if (input.size() >= 3 && static_cast<unsigned char>(input[0]) == 0xEF) {
    parser.index = 3;
  }
  Json root = parser.parseValue();
  parser.skip();
  if (parser.index != input.size()) {
    parser.fail("trailing data");
  }
  return root;
}

}  // namespace replora::test
