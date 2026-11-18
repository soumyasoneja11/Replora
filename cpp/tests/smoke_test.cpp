#include "replora/cache_policy.hpp"
#include "replora/types.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <iterator>
#include <optional>

namespace {

TEST(WorkloadType, StringRoundTrip) {
  const replora::WorkloadType values[] = {
      replora::WorkloadType::Sequential, replora::WorkloadType::Random,
      replora::WorkloadType::Looping,    replora::WorkloadType::Bursty,
      replora::WorkloadType::FrequencySkewed,
  };
  const char* names[] = {
      "sequential", "random", "looping", "bursty", "frequency-skewed",
  };

  ASSERT_EQ(std::size(values), std::size(names));

  for (std::size_t i = 0; i < std::size(values); ++i) {
    EXPECT_STREQ(replora::toString(values[i]), names[i]);
    const std::optional<replora::WorkloadType> parsed =
        replora::workloadFromString(names[i]);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(*parsed, values[i]);
    EXPECT_STREQ(replora::toString(*parsed), names[i]);
  }

  EXPECT_FALSE(replora::workloadFromString("").has_value());
  EXPECT_FALSE(replora::workloadFromString("Looping").has_value());
  EXPECT_FALSE(replora::workloadFromString("frequency_skewed").has_value());
}

}  // namespace
