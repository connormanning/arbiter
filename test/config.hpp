#include <arbiter/arbiter.hpp>
#include <arbiter/util/json.hpp>

class ARBITER_DLL Config
{
public:
    static const arbiter::json& get()
    {
        static Config config;
        return config.m_json;
    }

private:
    Config()
    {
        using namespace arbiter;

        std::string file("~/.arbiter/.test.json");

        if (const auto p = env("TEST_CONFIG_FILE"))
        {
            file = *p;
        }
        else if (const auto p = env("TEST_CONFIG_PATH"))
        {
            file = *p;
        }

        try
        {
            m_json = json::parse(drivers::Fs().get(file));
        }
        catch (...)
        {
            std::cout <<
                "No test config found.  " <<
                "Use file ~/.arbiter/.test.json or env TEST_CONFIG_FILE" <<
                std::endl;
        }
    }

    arbiter::json m_json;
};

