#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace arbiter
{
namespace crypto
{

std::vector<char> sha256(const std::vector<char>& data);
std::string sha256(const std::string& data);

std::string hmacSha256(const std::string& key, const std::string& data);

} // namespace crypto
} // namespace arbiter

