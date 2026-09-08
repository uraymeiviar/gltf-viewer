#pragma once

#include "core/String.h"
#include "core/Vector.h"
#include <cstdint>

namespace ux3d::renderer {
// the order of entries in this class depends directly on the values of the corresponding defines
// in the tonemapping shader, be sure to have them synchronized

enum class ToneMappingsEnum : uint32_t {
    None = 0u,
    ACES_NARKOWICZ,
    ACES_HILL,
    ACES_HILL_EXPOSURE_BOOST,
    PBR_NEUTRAL,
};

const static core::Vector<const core::string> toneMappingNames {
    "None",
    "ACES Filmic Tone Mapping (Narkowicz)",
    "ACES Filmic Tone Mapping (Hill)",
    "ACES Filmic Tone Mapping (Hill - Exposure Boost)",
    "PBR Neutral Tone Mapping",
};

inline core::string toneMappingToString(ToneMappingsEnum _toneMapping)
{
    const size_t index = static_cast<size_t>(_toneMapping);
    if (index >= toneMappingNames.size()) {
        core::logError("ToneMappingsEnum: tonemapping to string conversion failed.");
        return toneMappingNames.at(0);
    }

    return toneMappingNames.at(index);
}

inline ToneMappingsEnum toneMappingFromString(const core::string _toneMapping)
{
    for (size_t i = 0; i < toneMappingNames.size(); ++i) {
        if (toneMappingNames.at(i) == _toneMapping) {
            return static_cast<ToneMappingsEnum>(i);
        }
    }

    core::logError("ToneMappingsEnum: tonemapping from string conversion failed.");
    return ToneMappingsEnum::None;
}
}
