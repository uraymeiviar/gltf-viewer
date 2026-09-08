// Modified Version of https://stackoverflow.com/questions/180947/base64-decode-snippet-in-c

#pragma once

#include <string>
#include <vector>

namespace ux3d::utils::base64 {

std::string encode(const std::vector<uint8_t>& _buffer);
std::string encode(const uint8_t* _buffer, size_t _bufferLength);
void decode(const std::string& _encodedString, std::vector<uint8_t>& _outData);
std::string encode(const ::std::string& bindata);
std::string decode(const ::std::string& ascdata);
}
