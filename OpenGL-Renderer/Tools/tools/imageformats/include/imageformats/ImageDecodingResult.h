// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "core/EnumOps.h"
#include "core/Result.h"

namespace ux3d {
namespace imageformats {
    enum class ImageDecodingError : uint32_t {
        InvalidInput = core::DerivedResultStart,
        LoadFailure,
        InitializeFailure,
        AllocationFailure
    };

    using ImageDecodingResult = core::ResultT<ImageDecodingError>;
}
}
