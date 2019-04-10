#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/util/ini.hpp>
#endif

#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/util/util.hpp>
#endif

#ifdef ARBITER_CUSTOM_NAMESPACE
namespace ARBITER_CUSTOM_NAMESPACE
{
#endif

namespace arbiter
{
namespace ini
{

Contents parse(const std::string& s)
{
    Contents contents;

    Section section;

    const std::vector<std::string> lines;
    for (std::string line : split(s))
    {
        line = stripWhitespace(line);
        const std::size_t semiPos(line.find_first_of(';'));
        const std::size_t hashPos(line.find_first_of('#'));
        line = line.substr(0, std::min(semiPos, hashPos));

        if (line.size())
        {
            if (line.front() == '[' && line.back() == ']')
            {
                section = line.substr(1, line.size() - 2);
            }
            else
            {
                const std::size_t equals(line.find_first_of('='));
                if (equals != std::string::npos)
                {
                    const Key key(line.substr(0, equals));
                    const Val val(line.substr(equals + 1));
                    contents[section][key] = val;
                }
            }
        }
    }

    return contents;
}

} // namespace ini
} // namespace arbiter

#ifdef ARBITER_CUSTOM_NAMESPACE
}
#endif

