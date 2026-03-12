#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "arbiter.hpp"

using StringList = std::vector<std::string>;

using namespace arbiter;

Arbiter arb;
drivers::Http driver(arb.httpPool());

int get(const StringList& args)
{
    if (args.size() < 1 || args.size() > 2)
        return -1;

    const std::string& url = args[0];
    std::vector<char> v = driver.getBinary(url, http::Headers(), http::Query());
    if (args.size() == 2)
    {
        std::ofstream out(args[1], std::ios::binary | std::ios::trunc);
        if (!out)
        {
            std::cerr << "Couldn't open file '" << args[1] << "' for output.\n";
            return -1;
        }
        out.write(v.data(), v.size());
    }
    std::cerr << "Received = " << v.size() << " bytes.\n";
    return 0;
}

int size(const StringList& args)
{
    if (args.size() != 1)
        return -1;

    const std::string& url = args[0];
    std::unique_ptr<std::size_t> pSize = driver.tryGetSize(url);
    if (!pSize)
        std::cerr << "Couldn't get size for '" << url << "'.\n";
    else
        std::cerr << "Size: " << *pSize << "\n";
    return 0;
}

int put(const StringList& args)
{
    if (args.size() != 2)
        return -1;

    const std::string& file = args[0];
    const std::string& url = args[1];

    std::ifstream in(file, std::ios::binary);
    if (!in)
        throw ArbiterError("Couldn't open file '" + file + "' for input.\n");

    std::string buf(std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>());
    std::vector<char> data = driver.put(url, buf, http::Headers(), http::Query());
    if (data.size())
    {
        buf.assign(data.data(), data.size());
        std::cerr << "Received = " << buf << "!\n";
    }

    return 0;
}

int post(const StringList& args)
{
    if (args.size() != 2)
        return -1;

    const std::string& file = args[0];
    const std::string& url = args[1];

    std::ifstream in(file, std::ios::binary);
    if (!in)
        throw ArbiterError("Couldn't open file '" + file + "' for input.\n");

    std::string buf(std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>());
    driver.post(url, buf, http::Headers(), http::Query());

    return 0;
}

std::string getUserAgent()
{
    return "arbiter (1.0)";
}

int usage()
{
    std::cerr << "usage:\n";
    std::cerr << "\tarb get <url> [filename]\n";
    std::cerr << "\tarb size <url>\n";
    std::cerr << "\tarb put <filename> <url>\n";
    std::cerr << "\tarb post <filename> <url>\n";
    return -1;
}

int handleCommand(const std::string& cmd, const StringList& args)
{
    int status = -1;
    try
    {
        if (cmd == "get")
            status = get(args);
        else if (cmd == "put")
            status = put(args);
        else if (cmd == "post")
            status = post(args);
        else if (cmd == "size")
            status = size(args);
    }
    catch (const ArbiterError& err)
    {
        std::cerr << "Error: " << err.what() << ".\n";
        return -1;
    }

    if (status)
        return usage();
    return status;
}

int main(int argc, char *argv[])
{
    if (argc < 2)
        return usage();

    std::string cmd = argv[1];

    StringList args;
    for (size_t i = 2; i < (size_t)argc; ++i)
        args.push_back(argv[i]);

    handleCommand(cmd, args);
    return 0;
}
