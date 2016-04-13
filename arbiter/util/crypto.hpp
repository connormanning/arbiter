#pragma once

#include <string>
#include <vector>

namespace arbiter
{
namespace crypto
{

std::vector<char> hmacSha1(std::string key, std::string message);

// These aren't really crypto, so if this file grows a bit more they can move.
std::string encodeBase64(const std::vector<char>& data);
std::string encodeAsHex(const std::vector<char>& data);

} // namespace crypto
} // namespace arbiter

