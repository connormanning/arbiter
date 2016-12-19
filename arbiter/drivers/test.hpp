#pragma once

#include <numeric>

#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/drivers/fs.hpp>
#endif

#ifdef ARBITER_CUSTOM_NAMESPACE
namespace ARBITER_CUSTOM_NAMESPACE
{
#endif

namespace arbiter
{

class Arbiter;

namespace drivers
{

/** @brief A filesystem driver that acts as if it were remote for testing
 * purposes.
 */
class Test : public Fs
{
public:
    static std::unique_ptr<Test> create(const Json::Value& json)
    {
        return std::unique_ptr<Test>(new Test());
    }

    virtual std::string type() const override { return "test"; }
    virtual bool isRemote() const override { return true; }

private:
    virtual std::vector<std::string> glob(
            std::string path,
            bool verbose) const override
    {
        auto results(Fs::glob(path, verbose));
        for (auto& p : results) p = type() + "://" + p;
        return results;
    }
};

} // namespace drivers

} // namespace arbiter

#ifdef ARBITER_CUSTOM_NAMESPACE
}
#endif

