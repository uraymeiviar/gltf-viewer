// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "Platform.h"

#if defined(UX3D_IS_WINDOWS) && !defined(UX3D_NO_WINDOWS_HEADER)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>

#endif // UX3D_IS_WINDOWS && !UX3D_NO_WINDOWS_HEADER
