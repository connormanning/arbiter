#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/util/transforms.hpp>
#endif

#include <cstdint>
#include <stdexcept>

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

    static unsigned int posOfChar(const unsigned char chr) {
     //
     // Return the position of chr within base64_encode()
     //
    
        if      (chr >= 'A' && chr <= 'Z') return chr - 'A';
        else if (chr >= 'a' && chr <= 'z') return chr - 'a' + ('Z' - 'A')               + 1;
        else if (chr >= '0' && chr <= '9') return chr - '0' + ('Z' - 'A') + ('z' - 'a') + 2;
        else if (chr == '+' || chr == '-') return 62; // Be liberal with input and accept both url ('-') and non-url ('+') base 64 characters (
        else if (chr == '/' || chr == '_') return 63; // Ditto for '/' and '_'
        else
     //
     // 2020-10-23: Throw std::exception rather than const char*
     //(Pablo Martin-Gomez, https://github.com/Bouska)
     //
        throw std::runtime_error("Input is not valid base64-encoded data.");
    }

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

std::string decodeBase64(const std::string & input)
{
    size_t lengthOfString = input.length();
    size_t pos = 0;

 //
 // The approximate length (bytes) of the decoded string might be one or
 // two bytes smaller, depending on the amount of trailing equal signs
 // in the encoded string. This approximation is needed to reserve
 // enough space in the string to be returned.
 //
    size_t approxLengthOfDecodedString = lengthOfString / 4 * 3;
    std::string ret;
    ret.reserve(approxLengthOfDecodedString);

    while (pos < lengthOfString) {

       unsigned int posOfChar1 = posOfChar(input[pos+1] );

       ret.push_back(static_cast<std::string::value_type>( ( (posOfChar(input[pos+0]) ) << 2 ) + ( (posOfChar1 & 0x30 ) >> 4)));

       if (input[pos+2] != '=' && input[pos+2] != '.') { // accept URL-safe base 64 strings, too, so check for '.' also.

          unsigned int posOfChar2 = posOfChar(input[pos+2] );
          ret.push_back(static_cast<std::string::value_type>( (( posOfChar1 & 0x0f) << 4) + (( posOfChar2 & 0x3c) >> 2)));

          if (input[pos+3] != '=' && input[pos+3] != '.') {
             ret.push_back(static_cast<std::string::value_type>( ( (posOfChar2 & 0x03 ) << 6 ) + posOfChar(input[pos+3])));
          }
       }

       pos += 4;
    }

    return ret;
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

