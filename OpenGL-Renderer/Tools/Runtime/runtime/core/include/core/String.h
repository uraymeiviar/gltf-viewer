// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include <cstring>
#include <locale>
#include <string>

namespace ux3d {
namespace core {
    template <typename T>
    using basic_string = std::basic_string<T>;

    using string = std::string;
    using wstring = std::wstring;

    template <class char_t>
    inline basic_string<char_t> toLower(const basic_string<char_t>& _String, const std::locale& loc = std::locale())
    {
        basic_string<char_t> result(_String);
        for (char_t& c : result) {
            c = std::tolower(c, loc);
        }
        return result;
    }

    template <class char_t>
    inline basic_string<char_t> toUpper(const basic_string<char_t>& _String, const std::locale& loc = std::locale())
    {
        basic_string<char_t> result(_String);
        for (char_t& c : result) {
            c = std::toupper(c, loc);
        }
        return result;
    }

    template <class char_t>
    inline bool startsWith(const basic_string<char_t>& _FullString, const basic_string<char_t>& _Prefix)
    {
        return _FullString.length() >= _Prefix.length() && (0 == _FullString.compare(0, _Prefix.length(), _Prefix));
    }

    template <class char_t>
    inline bool startsWith(const basic_string<char_t>& _FullString, const char_t* _pPrefix)
    {
        const size_t len = strlen(_pPrefix);
        return _FullString.length() >= len && (0 == _FullString.compare(0, len, _pPrefix));
    }

    template <class char_t>
    inline bool endsWith(const basic_string<char_t>& _FullString, const basic_string<char_t>& _Suffix)
    {
        return _FullString.length() >= _Suffix.length() && (0 == _FullString.compare(_FullString.length() - _Suffix.length(), _Suffix.length(), _Suffix));
    }

    template <class char_t>
    inline bool endsWith(const basic_string<char_t>& _FullString, const char_t* _pSuffix)
    {
        const size_t len = strlen(_pSuffix);
        return _FullString.length() >= len && (0 == _FullString.compare(_FullString.length() - len, len, _pSuffix));
    }

    template <class char_t>
    inline bool contains(const basic_string<char_t>& _string, const char_t* _pSubstring)
    {
        return _string.find(_pSubstring) != std::string::npos;
    }

    template <class char_t>
    inline basic_string<char_t> replaceAll(const basic_string<char_t>& _subject, const std::string& search,
        const std::string& _replace)
    {
        basic_string<char_t> result(_subject);
        if (search.empty()) {
            return result;
        }
        size_t pos = 0;
        while ((pos = result.find(search, pos)) != std::string::npos) {
            result.replace(pos, search.length(), _replace);
            pos += _replace.length();
        }
        return result;
    }
}
}
