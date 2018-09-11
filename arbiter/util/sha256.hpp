#pragma once

#include <cstddef>
#include <string>
#include <vector>

#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/util/exports.hpp>

#ifndef ARBITER_EXTERNAL_JSON
#include <arbiter/third/json/json.hpp>
#endif

#endif


// SHA256 implementation adapted from:
//      https://github.com/B-Con/crypto-algorithms
// HMAC:
//      https://en.wikipedia.org/wiki/Hash-based_message_authentication_code

#ifdef ARBITER_CUSTOM_NAMESPACE
namespace ARBITER_CUSTOM_NAMESPACE
{
#endif

namespace arbiter
{
namespace crypto
{

ARBITER_DLL std::vector<char> sha256(const std::vector<char>& data);
ARBITER_DLL std::string sha256(const std::string& data);

ARBITER_DLL std::string hmacSha256(
        const std::string& key,
        const std::string& data);

} // namespace crypto
} // namespace arbiter

#ifdef ARBITER_CUSTOM_NAMESPACE
}
#endif

