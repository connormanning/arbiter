#pragma once

#include <cstddef>
#include <string>
#include <vector>

// MD5 implementation adapted from:
//      https://github.com/B-Con/crypto-algorithms

namespace arbiter
{
namespace crypto
{

std::string md5(const std::string& data);

} // namespace crypto
} // namespace arbiter

