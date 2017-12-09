#pragma once

#include <string>
#include <vector>

#ifdef ARBITER_CUSTOM_NAMESPACE
namespace ARBITER_CUSTOM_NAMESPACE
{
#endif

namespace arbiter
{
namespace crypto
{

std::string encodeBase64(const std::vector<char>& data, bool pad = true);
std::string encodeBase64(const std::string& data, bool pad = true);

std::string encodeAsHex(const std::vector<char>& data);
std::string encodeAsHex(const std::string& data);

} // namespace crypto
} // namespace arbiter

#ifdef ARBITER_CUSTOM_NAMESPACE
}
#endif

