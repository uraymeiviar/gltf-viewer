#include "utils/Base64.h"
#include <limits>
#include <stdexcept>

using namespace ux3d;
using namespace ux3d::utils;

static const char b64_table[65] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static const char reverse_table[128] = {
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 62, 64, 64, 64, 63,
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 64, 64, 64, 64, 64, 64,
    64, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 64, 64, 64, 64, 64,
    64, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 64, 64, 64, 64, 64
};

std::string base64::encode(const std::vector<uint8_t>& _buffer)
{
    if (_buffer.empty()) {
        return "";
    }
    return base64::encode(std::string(_buffer.begin(), _buffer.end()));
}

std::string base64::encode(const uint8_t* _buffer, size_t _bufferLength)
{
    if (_bufferLength == 0) {
        return "";
    }
    std::string input;
    input.resize(_bufferLength);
    for (int i = 0; i < _bufferLength; ++i) {
        input[i] = *(_buffer + i);
    }
    std::string result = base64::encode(input);
    return result;
}

void base64::decode(const std::string& _encodedString, std::vector<uint8_t>& _outData)
{
    std::string result = decode(_encodedString);
    _outData.clear();
    _outData.reserve(result.size());
    for (char c : result) {
        _outData.push_back(c);
    }
}

std::string base64::encode(const std::string& bindata)
{

    if (bindata.size() > (std::numeric_limits<std::string::size_type>::max() / 4u) * 3u) {
        throw ::std::length_error("Converting too large a string to Base64.");
    }

    const ::std::size_t binlen = bindata.size();
    // Use = signs so the end is properly padded.
    std::string retval((((binlen + 2) / 3) * 4), '=');
    ::std::size_t outpos = 0;
    int bits_collected = 0;
    unsigned int accumulator = 0;
    const std::string::const_iterator binend = bindata.end();

    for (std::string::const_iterator i = bindata.begin(); i != binend; ++i) {
        accumulator = (accumulator << 8) | (*i & 0xffu);
        bits_collected += 8;
        while (bits_collected >= 6) {
            bits_collected -= 6;
            retval[outpos++] = b64_table[(accumulator >> bits_collected) & 0x3fu];
        }
    }
    if (bits_collected > 0) { // Any trailing bits that are missing.
        accumulator <<= 6 - bits_collected;
        retval[outpos++] = b64_table[accumulator & 0x3fu];
    }
    return retval;
}

std::string base64::decode(const std::string& ascdata)
{
    std::string retval;
    const std::string::const_iterator last = ascdata.end();
    int bits_collected = 0;
    unsigned int accumulator = 0;

    for (std::string::const_iterator i = ascdata.begin(); i != last; ++i) {
        const int c = *i;
        if (::std::isspace(c) || c == '=') {
            // Skip whitespace and padding. Be liberal in what you accept.
            continue;
        }
        if ((c > 127) || (c < 0) || (reverse_table[c] > 63)) {
            throw ::std::invalid_argument("This contains characters not legal in a Base64 encoded string.");
        }
        accumulator = (accumulator << 6) | reverse_table[c];
        bits_collected += 6;
        if (bits_collected >= 8) {
            bits_collected -= 8;
            retval += static_cast<char>((accumulator >> bits_collected) & 0xffu);
        }
    }
    return retval;
}
