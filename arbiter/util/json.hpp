#pragma once

#include <arbiter/third/json/json.hpp>

#ifdef ARBITER_CUSTOM_NAMESPACE
namespace ARBITER_CUSTOM_NAMESPACE
{
#endif

namespace arbiter
{

using json = nlohmann::json;

// Work around:
// https://github.com/nlohmann/json/issues/709
// https://github.com/google/googletest/pull/1186
inline void PrintTo(const json& j, std::ostream* os) { *os << j.dump(); }

// Merge B into A, without overwriting any keys from A.
inline json merge(const json& a, const json& b)
{
    json out(a);
    if (out.is_null()) out = json::object();

    if (!b.is_null())
    {
        if (b.is_object())
        {
            for (const auto& p : b.items())
            {
                // If A doesn't have this key, then set it to B's value.
                // If A has the key but it's an object, then recursively
                // merge.
                // Otherwise A already has a value here that we won't
                // overwrite.
                const std::string& key(p.key());
                const json& val(p.value());

                if (!out.count(key)) out[key] = val;
                else if (out[key].is_object()) merge(out[key], val);
            }
        }
        else
        {
            out = b;
        }
    }

    return out;
}

} // namespace arbiter

#ifdef ARBITER_CUSTOM_NAMESPACE
}
#endif
