#include <arbiter/arbiter.hpp>

class ARBITER_DLL Config
{
public:
    static const Json::Value& get()
    {
        static Config config;
        return config.m_json;
    }

private:
    Config()
    {
        std::string file("~/.arbiter/.test.json");

        if (const auto p = arbiter::util::env("TEST_CONFIG_FILE"))
        {
            file = *p;
        }
        else if (const auto p = arbiter::util::env("TEST_CONFIG_PATH"))
        {
            file = *p;
        }

        try
        {
            m_json = arbiter::util::parse(arbiter::drivers::Fs().get(file));
        }
        catch (...)
        {
            std::cout <<
                "No test config found.  " <<
                "Use file ~/.arbiter/.test.json or env TEST_CONFIG_FILE" <<
                std::endl;
        }
    }

    Json::Value m_json;
};

