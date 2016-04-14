#pragma once

#include <string>
#include <vector>

namespace arbiter
{
namespace crypto
{

std::vector<char> hmacSha1(const std::string& key, const std::string& data);

// These aren't really crypto, so if this file grows a bit more they can move.
std::string encodeBase64(const std::vector<char>& data);
std::string encodeAsHex(const std::vector<char>& data);
std::string encodeAsHex(const std::string& data);

} // namespace crypto
} // namespace arbiter

