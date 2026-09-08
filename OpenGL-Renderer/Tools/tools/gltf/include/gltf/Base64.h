// Modified Version of https://stackoverflow.com/questions/180947/base64-decode-snippet-in-c
// - naming convention
// - using core types

#pragma once

#include "core/String.h"
#include "core/Vector.h"

namespace ux3d {
class Base64 {
public:
    static core::string encode(const core::Vector<uint8_t>& _buffer);
    static core::string encode(core::MemoryRange<const uint8_t>& _buffer);
    static core::string encode(const uint8_t* _buffer, size_t _bufferLength);
    static void decode(core::string& _encodedString, core::Vector<uint8_t>& _outData);
    static std::string encode(const ::std::string& bindata);
    static std::string decode(const ::std::string& ascdata);
};
}
