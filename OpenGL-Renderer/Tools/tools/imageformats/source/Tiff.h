// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

// Baseline TIFF only. Kept independent of the renderer so fixtures can compare every
// decoded byte with an independent encoder without a GPU or colour conversion.
namespace ux3d::imageformats::tiff {
struct Pixels {
    uint32_t width = 0, height = 0;
    std::vector<uint8_t> rgba;
};

class Reader {
    std::span<const uint8_t> bytes;
    bool little;
    void require(bool ok, const char* reason) const
    {
        if (!ok) throw std::runtime_error(reason);
    }
    void range(size_t offset, size_t count) const
    {
        require(offset <= bytes.size() && count <= bytes.size() - offset, "truncated TIFF data/offset");
    }
    uint32_t number(size_t offset, size_t count) const
    {
        range(offset, count);
        uint32_t value = 0;
        for (size_t i = 0; i < count; ++i)
            value |= uint32_t(bytes[offset + i]) << (8 * (little ? i : count - 1 - i));
        return value;
    }
    std::vector<uint32_t> tag(uint16_t id, uint32_t fallback) const
    {
        const size_t ifd = number(4, 4);
        const size_t count = number(ifd, 2);
        range(ifd + 2, count * 12 + 4);
        for (size_t i = 0; i < count; ++i) {
            const size_t entry = ifd + 2 + i * 12;
            if (number(entry, 2) != id) continue;
            const uint32_t type = number(entry + 2, 2), n = number(entry + 4, 4);
            require(type == 1 || type == 3 || type == 4, "unsupported TIFF tag type");
            const size_t size = type == 3 ? 2 : type == 4 ? 4 : 1;
            require(n > 0 && n <= bytes.size() / size, "invalid TIFF tag count");
            // Scalar metadata is inspected before acquiring the decode lease. Do
            // not let a forged width/height count allocate an image-sized vector.
            if (id != 273 && id != 279)
                require(n <= (id == 258 || id == 339 ? 4u : 1u), "invalid TIFF scalar/sample tag count");
            const size_t offset = size * n <= 4 ? entry + 8 : number(entry + 8, 4);
            range(offset, size * n);
            std::vector<uint32_t> result(n);
            for (size_t j = 0; j < n; ++j) result[j] = number(offset + j * size, size);
            return result;
        }
        return {fallback};
    }
    uint32_t scalar(uint16_t id, uint32_t fallback) const { return tag(id, fallback)[0]; }

    std::vector<uint8_t> unpack(size_t offset, size_t length, size_t expected, uint32_t compression) const
    {
        range(offset, length);
        const auto input = bytes.subspan(offset, length);
        std::vector<uint8_t> out;
        out.reserve(expected);
        auto emit = [&](uint8_t b) {
            require(out.size() < expected, "TIFF strip expands beyond row capacity");
            out.push_back(b);
        };
        if (compression == 1) {
            require(length >= expected, "short uncompressed TIFF strip");
            out.assign(input.begin(), input.begin() + expected);
        } else if (compression == 32773) {
            size_t pos = 0;
            while (pos < input.size() && out.size() < expected) {
                const uint8_t encoded = input[pos++];
                const int control = encoded < 128 ? encoded : int(encoded) - 256;
                if (control >= 0) {
                    require(size_t(control + 1) <= input.size() - pos, "truncated TIFF PackBits literal");
                    for (int j = 0; j <= control; ++j) emit(input[pos++]);
                } else if (control != -128) {
                    require(pos < input.size(), "truncated TIFF PackBits repeat");
                    const uint8_t b = input[pos++];
                    for (int j = 0; j < 1 - control; ++j) emit(b);
                }
            }
        } else {
            // TIFF LZW: MSB-first, clear 256, EOI 257, early code-width change.
            std::array<uint16_t, 4096> prefix{};
            std::array<uint8_t, 4096> suffix{}, stack{};
            size_t bit = 0;
            int width = 9, next = 258, previous = -1;
            uint8_t first = 0;
            bool ended = false;
            while (bit + width <= input.size() * 8) {
                int code = 0;
                for (int j = 0; j < width; ++j, ++bit)
                    code = (code << 1) | ((input[bit / 8] >> (7 - bit % 8)) & 1);
                if (code == 256) { width = 9; next = 258; previous = -1; continue; }
                if (code == 257) { ended = true; break; }
                const int current = code;
                size_t count = 0;
                if (code == next && previous >= 0) {
                    stack[count++] = first;
                    code = previous;
                } else require(code < next, "invalid TIFF LZW code");
                while (code >= 258) {
                    require(code < next && count < stack.size() - 1, "invalid TIFF LZW dictionary chain");
                    stack[count++] = suffix[code];
                    code = prefix[code];
                }
                require(code < 256, "invalid TIFF LZW literal");
                first = uint8_t(code);
                stack[count++] = first;
                while (count) emit(stack[--count]);
                if (previous >= 0 && next < 4096) {
                    prefix[next] = uint16_t(previous);
                    suffix[next++] = first;
                    if (width < 12 && next == (1 << width) - 1) ++width;
                }
                previous = current;
            }
            require(ended, "missing TIFF LZW end code");
        }
        require(out.size() == expected, "short decoded TIFF strip");
        return out;
    }
public:
    explicit Reader(std::span<const uint8_t> data) : bytes(data), little(data.size() >= 2 && data[0] == 'I') {}
    std::array<uint32_t, 2> dimensions() const
    {
        const auto w = scalar(256, 0), h = scalar(257, 0);
        require(w && h && uint64_t(w) * h <= (uint64_t(1) << 28), "invalid/oversized TIFF dimensions (1 GiB limit)");
        return {w, h};
    }
    Pixels decode() const
    {
        require(bytes.size() >= 8 && ((bytes[0] == 'I' && bytes[1] == 'I') || (bytes[0] == 'M' && bytes[1] == 'M')),
            "invalid TIFF byte order");
        require(number(2, 2) == 42, "unsupported TIFF version (BigTIFF)");
        require(scalar(322, 0) == 0 && scalar(324, 0) == 0, "unsupported tiled TIFF");
        require(scalar(284, 1) == 1, "unsupported TIFF planar configuration (separate planes)");
        require(scalar(274, 1) == 1, "unsupported TIFF orientation (expected top-left)");
        require(scalar(266, 1) == 1, "unsupported TIFF FillOrder");
        for (auto format : tag(339, 1)) require(format == 1, "unsupported TIFF sample format (float/signed)");
        const auto compression = scalar(259, 1);
        require(compression == 1 || compression == 5 || compression == 32773,
            "unsupported TIFF compression (expected none, LZW or PackBits)");
        const auto predictor = scalar(317, 1);
        require(predictor == 1 || predictor == 2, "unsupported TIFF predictor");
        const auto photo = scalar(262, 1), samples = scalar(277, 1);
        require((photo <= 1 && (samples == 1 || samples == 2)) || (photo == 2 && (samples == 3 || samples == 4)),
            "unsupported TIFF photometric interpretation/channel count");
        const auto bits = tag(258, 1);
        require(bits.size() == 1 || bits.size() == samples, "invalid TIFF BitsPerSample count");
        for (auto b : bits) require((b == 8 || b == 16) && b == bits[0], "unsupported TIFF bits per sample (expected uniform 8/16)");
        const auto extra = scalar(338, 0);
        require(extra <= 2, "unsupported TIFF extra sample type");
        Pixels result;
        result.width = scalar(256, 0); result.height = scalar(257, 0);
        const auto rows = scalar(278, result.height);
        require(result.width && result.height && rows, "invalid TIFF dimensions/RowsPerStrip");
        // Bound hostile dimensions before allocation; at most 1 GiB of RGBA8 per image.
        require(uint64_t(result.width) * result.height <= (uint64_t(1) << 28), "TIFF decoded image exceeds 1 GiB limit");
        const auto offsets = tag(273, 0), lengths = tag(279, 0);
        const size_t strips = (uint64_t(result.height) + rows - 1) / rows;
        require(offsets.size() == strips && lengths.size() == strips, "invalid TIFF strip table");
        result.rgba.resize(size_t(result.width) * result.height * 4);
        const size_t bytesPerSample = bits[0] / 8;
        const size_t stride = size_t(result.width) * samples;
        for (size_t strip = 0; strip < strips; ++strip) {
            const size_t rowStart = strip * rows, rowCount = std::min<size_t>(rows, result.height - rowStart);
            auto raw = unpack(offsets[strip], lengths[strip], rowCount * stride * bytesPerSample, compression);
            std::vector<uint16_t> line(stride);
            for (size_t y = 0; y < rowCount; ++y) {
                for (size_t s = 0; s < stride; ++s) {
                    const size_t p = (y * stride + s) * bytesPerSample;
                    uint32_t value = raw[p];
                    if (bytesPerSample == 2) value = little ? value | uint32_t(raw[p + 1]) << 8 : value << 8 | raw[p + 1];
                    if (predictor == 2 && s >= samples) value += line[s - samples];
                    line[s] = uint16_t(value & (bytesPerSample == 1 ? 255 : 65535));
                }
                for (size_t x = 0; x < result.width; ++x) {
                    const bool alpha = samples == (photo == 2 ? 4u : 2u);
                    const uint32_t maximum = bits[0] == 8 ? 255 : 65535;
                    const uint32_t a = alpha ? line[x * samples + samples - 1] : maximum;
                    const size_t dest = ((rowStart + y) * result.width + x) * 4;
                    for (size_t c = 0; c < 3; ++c) {
                        uint32_t v = line[x * samples + (photo == 2 ? c : 0)];
                        if (photo == 0) v = maximum - v;
                        if (alpha && extra == 1) v = a ? uint32_t(std::min<uint64_t>(maximum, uint64_t(v) * maximum / a)) : 0;
                        result.rgba[dest + c] = uint8_t(bits[0] == 16 ? v >> 8 : v);
                    }
                    result.rgba[dest + 3] = uint8_t(bits[0] == 16 ? a >> 8 : a);
                }
            }
        }
        return result;
    }
};

inline bool decode(std::span<const uint8_t> bytes, Pixels& out, std::string& reason)
{
    try { out = Reader(bytes).decode(); return true; }
    catch (const std::exception& error) { reason = error.what(); return false; }
}
} // namespace ux3d::imageformats::tiff
