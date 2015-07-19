#pragma once

#include <string>
#include <vector>

namespace arbiter
{
namespace crypto
{

std::vector<char> hmacSha1(std::string key, std::string message);

} // namespace crypto
} // namespace arbiter

