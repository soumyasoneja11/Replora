#pragma once

#include "replora/types.hpp"

#include <string>

namespace replora {

// Stable serializer shared by the native CLI and the WASM build.
// Field order and names are documented in docs/result-schema.md.
std::string toJson(const SimulationResult& result);

}  // namespace replora
