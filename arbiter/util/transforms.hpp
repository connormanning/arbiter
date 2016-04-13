#pragma once

#include <string>
#include <vector>

namespace arbiter
{
namespace crypto
{

std::string encodeBase64(const std::vector<char>& data);
std::string encodeBase64(const std::string& data);

std::string encodeAsHex(const std::vector<char>& data);
std::string encodeAsHex(const std::string& data);

} // namespace crypto
} // namespace arbiter

