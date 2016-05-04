#pragma once

#include <cstddef>
#include <string>
#include <vector>

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

std::vector<char> sha256(const std::vector<char>& data);
std::string sha256(const std::string& data);

std::string hmacSha256(const std::string& key, const std::string& data);

} // namespace crypto
} // namespace arbiter

#ifdef ARBITER_CUSTOM_NAMESPACE
}
#endif

