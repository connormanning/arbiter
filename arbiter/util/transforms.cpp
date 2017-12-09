#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/util/transforms.hpp>
#endif

#include <cstdint>

#ifdef ARBITER_CUSTOM_NAMESPACE
namespace ARBITER_CUSTOM_NAMESPACE
{
#endif

namespace arbiter
{
namespace crypto
{
namespace
{
    const std::string base64Vals(
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/");

    const std::string hexVals("0123456789abcdef");
} // unnamed namespace

std::string encodeBase64(const std::vector<char>& data, const bool pad)
{
    std::vector<uint8_t> input;
    for (std::size_t i(0); i < data.size(); ++i)
    {
        char c(data[i]);
        input.push_back(*reinterpret_cast<uint8_t*>(&c));
    }

    const std::size_t fullSteps(data.size() / 3);
    const std::size_t remainder(data.size() % 3);

    while (input.size() % 3) input.push_back(0);
    uint8_t* pos(input.data());

    std::string output(fullSteps * 4, '_');
    std::size_t outIndex(0);

    const uint32_t mask(0x3F);

    for (std::size_t i(0); i < fullSteps; ++i)
    {
        uint32_t chunk((*pos) << 16 | *(pos + 1) << 8 | *(pos + 2));

        output[outIndex++] = base64Vals[(chunk >> 18) & mask];
        output[outIndex++] = base64Vals[(chunk >> 12) & mask];
        output[outIndex++] = base64Vals[(chunk >>  6) & mask];
        output[outIndex++] = base64Vals[chunk & mask];

        pos += 3;
    }

    if (remainder)
    {
        uint32_t chunk(*(pos) << 16 | *(pos + 1) << 8 | *(pos + 2));

        output.push_back(base64Vals[(chunk >> 18) & mask]);
        output.push_back(base64Vals[(chunk >> 12) & mask]);
        if (remainder == 2) output.push_back(base64Vals[(chunk >> 6) & mask]);

        if (pad)
        {
            while (output.size() % 4) output.push_back('=');
        }
    }

    return output;
}

std::string encodeBase64(const std::string& input, const bool pad)
{
    return encodeBase64(std::vector<char>(input.begin(), input.end()), pad);
}

std::string encodeAsHex(const std::vector<char>& input)
{
    std::string output;
    output.reserve(input.size() * 2);

    uint8_t u(0);

    for (const char c : input)
    {
        u = *reinterpret_cast<const uint8_t*>(&c);
        output.push_back(hexVals[u >> 4]);
        output.push_back(hexVals[u & 0x0F]);
    }

    return output;
}

std::string encodeAsHex(const std::string& input)
{
    return encodeAsHex(std::vector<char>(input.begin(), input.end()));
}

} // namespace crypto
} // namespace arbiter

#ifdef ARBITER_CUSTOM_NAMESPACE
}
#endif

