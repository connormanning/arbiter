#pragma once

#include "mjson.hpp"
#include <json/json.h> // TODO Remove.

namespace arbiter { using json = nlohmann::json; }

// Work around:
// https://github.com/nlohmann/json/issues/709
// https://github.com/google/googletest/pull/1186
namespace nlohmann
{
inline void PrintTo(const json& j, std::ostream* os) { *os << j.dump(); }
}

