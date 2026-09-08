// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "String.h"
#include <stdint.h>

namespace ux3d {
namespace core {
    namespace detail {
        constexpr uint64_t g_seed = 14695981039346656037ULL;
        constexpr uint64_t g_prime = 1099511628211ULL;

        inline uint64_t fnv1a_hash_impl(const char* _pData, uint64_t _seed) noexcept
        {
            if (_pData != nullptr) {
                for (; *_pData != 0; ++_pData) {
                    _seed ^= uint64_t(*_pData);
                    _seed *= g_prime;
                }
            }
            return _seed;
        }

        inline uint64_t fnv1a_hash_impl(const uint8_t* _pData, uint64_t _seed) noexcept
        {
            if (_pData != nullptr) {
                for (; *_pData != 0; ++_pData) {
                    _seed ^= uint64_t(*_pData);
                    _seed *= g_prime;
                }
            }
            return _seed;
        }

        inline uint64_t fnv1a_hash_impl(const uint8_t* _pData, const size_t _size, uint64_t _seed) noexcept
        {
            // FNV-1a hash function for bytes in [_pData, _pData + _size)
            if (_pData != nullptr) {
                for (size_t i = 0; i < _size; ++i) {
                    _seed ^= uint64_t(_pData[i]);
                    _seed *= g_prime;
                }
            }
            return _seed;
        }
    }

    inline uint64_t hash(const char* _pStr, const uint64_t _seed = detail::g_seed) noexcept
    {
        return detail::fnv1a_hash_impl(_pStr, _seed);
    }

    inline uint64_t hash(const uint8_t* _pStr, const uint64_t _seed = detail::g_seed) noexcept
    {
        return detail::fnv1a_hash_impl(_pStr, _seed);
    }

    template <typename char_t>
    inline uint64_t hash(const basic_string<char_t>& _Str, const uint64_t _seed = detail::g_seed) noexcept
    {
        return detail::fnv1a_hash_impl(reinterpret_cast<const uint8_t*>(_Str.data()), sizeof(char_t) * _Str.size(), _seed);
    }

    template <typename T>
    inline uint64_t hash(const T& _Val, const uint64_t _seed = detail::g_seed) noexcept
    {
        return detail::fnv1a_hash_impl(reinterpret_cast<const uint8_t*>(std::addressof(_Val)), sizeof(T), _seed);
    }

    template <typename U>
    inline uint64_t hashAllWithSeed(const uint64_t _seed, const U& _Val1) noexcept
    {
        return hash(_Val1, _seed);
    }

    template <typename U, typename... V>
    inline uint64_t hashAllWithSeed(const uint64_t _seed, const U& _Val1, const V&... _Tail) noexcept
    {
        return hashAllWithSeed(hash(_Val1, _seed), _Tail...);
    }

    template <typename... V>
    inline uint64_t hashAll(const V&... _Tail) noexcept
    {
        return hashAllWithSeed(detail::g_seed, _Tail...);
    }

    // specialize this class for any custom type or container you want to be hash able, see VectorHash.h for example
    template <typename T>
    struct Hash {
        uint64_t operator()(const T& t, const uint64_t _seed = detail::g_seed) const noexcept
        {
            return hash(t, _seed); // resort to default implementation if no specialization was matched
        }
    };

    class Hasher {
    public:
        inline explicit Hasher(const uint64_t _hash = 0u) noexcept
            : m_hash(_hash)
        {
        }
        inline operator uint64_t() const noexcept { return m_hash; }

        template <class T>
        inline Hasher& operator<<(const T& _val) noexcept
        {
            Hash<T> func;
            m_hash = func(_val, m_hash);
            return *this;
        }

        template <class T>
        inline Hasher& operator()(const T& _val) noexcept
        {
            Hash<T> func;
            m_hash = func(_val, m_hash);
            return *this;
        }

        template <class T, class... Args>
        inline Hasher& operator()(const T& _val, const Args&... _tail) noexcept
        {
            operator()(_val);

            if constexpr (sizeof...(Args) > 0) {
                operator()(_tail...);
            }

            return *this;
        }

        inline Hasher& operator=(const uint64_t& _hash) noexcept
        {
            m_hash = _hash;
            return *this;
        }

        // override hash
        inline void setHash(const uint64_t _hash) noexcept { m_hash = _hash; }

        inline const uint64_t& getHash() const noexcept { return m_hash; }

    private:
        uint64_t m_hash;
    };
} // !core
} // !ux3d
