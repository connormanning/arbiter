#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <arbiter/arbiter.hpp>
#include <arbiter/util/json.hpp>

#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/util/exports.hpp>
#endif

#include "config.hpp"

#ifdef ARBITER_CUSTOM_NAMESPACE
namespace ARBITER_CUSTOM_NAMESPACE
{
#endif
using namespace arbiter;

int main() {
    Arbiter a;
    std::string path("od://Documents/autzen.las");
    auto data = a.get(path);
    auto size = a.tryGetSize(path);

    return 0;
}

#ifdef ARBITER_CUSTOM_NAMESPACE
}
#endif