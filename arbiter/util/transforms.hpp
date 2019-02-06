#pragma once

#include <string>
#include <vector>

#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/util/exports.hpp>
#endif


#ifdef ARBITER_CUSTOM_NAMESPACE
namespace ARBITER_CUSTOM_NAMESPACE
{
#endif

namespace arbiter
{
namespace crypto
{

ARBITER_DLL std::string encodeBase64(
        const std::vector<char>& data,
        bool pad = true);
ARBITER_DLL std::string encodeBase64(const std::string& data, bool pad = true);

ARBITER_DLL std::string encodeAsHex(const std::vector<char>& data);
ARBITER_DLL std::string encodeAsHex(const std::string& data);

} // namespace crypto
} // namespace arbiter

#ifdef ARBITER_CUSTOM_NAMESPACE
}
#endif

