#pragma once

#include <map>
#include <string>
#include <vector>

#ifdef ARBITER_CUSTOM_NAMESPACE
namespace ARBITER_CUSTOM_NAMESPACE
{
#endif

namespace arbiter
{
namespace ini
{

using Section = std::string;
using Key = std::string;
using Val = std::string;
using Contents = std::map<Section, std::map<Key, Val>>;

Contents parse(const std::string& s);

} // namespace ini

} // namespace arbiter

#ifdef ARBITER_CUSTOM_NAMESPACE
}
#endif

