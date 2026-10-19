// Copyright (c) 2026 UX3D GmbH / Glider Contributors. All rights reserved.

#include <cstdint>
#include <cstdlib>
#include <cstring>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

#define TINYEXR_USE_MINIZ 1
#define TINYEXR_IMPLEMENTATION
#include <tinyexr.h>
