#pragma once

#include <cstddef>
#include <string>
#include <vector>

#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/util/exports.hpp>
#endif

// MD5 implementation adapted from:
//      https://github.com/B-Con/crypto-algorithms

#ifdef ARBITER_CUSTOM_NAMESPACE
namespace ARBITER_CUSTOM_NAMESPACE
{
#endif

namespace arbiter
{
namespace crypto
{

ARBITER_DLL std::string md5(const std::string& data);

} // namespace crypto
} // namespace arbiter

#ifdef ARBITER_CUSTOM_NAMESPACE
}
#endif

