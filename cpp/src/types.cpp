#include "replora/types.hpp"

namespace replora {

const char* toString(WorkloadType workload) {
  switch (workload) {
    case WorkloadType::Sequential:
      return "sequential";
    case WorkloadType::Random:
      return "random";
    case WorkloadType::Looping:
      return "looping";
    case WorkloadType::Bursty:
      return "bursty";
    case WorkloadType::FrequencySkewed:
      return "frequency-skewed";
  }
  return "";
}

std::optional<WorkloadType> workloadFromString(std::string_view text) {
  if (text == "sequential") {
    return WorkloadType::Sequential;
  }
  if (text == "random") {
    return WorkloadType::Random;
  }
  if (text == "looping") {
    return WorkloadType::Looping;
  }
  if (text == "bursty") {
    return WorkloadType::Bursty;
  }
  if (text == "frequency-skewed") {
    return WorkloadType::FrequencySkewed;
  }
  return std::nullopt;
}

}  // namespace replora
