// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "Platform.h"

#if defined(UX3D_IS_MSVC)
#include <intrin.h>
#elif !defined(UX3D_IS_CLANG) && defined(UX3D_IS_POSIX)
#include <signal.h>
#endif

// https://github.com/nemequ/portable-snippets/blob/master/debug-trap/debug-trap.h

namespace ux3d {
namespace core {
    inline void debugBreak()
    {
#if defined(UX3D_IS_MSVC)

        __debugbreak();

#elif defined(UX3D_IS_CLANG)

        __builtin_debugtrap();

#elif defined(UX3D_IS_POSIX)

#if defined(SIGTRAP)
        raise(SIGTRAP);
#elif defined(SIGABRT)
        raise(SIGABRT);
#endif // !SIGABRT

#endif //!UX3D_IS_POSIX
    }
} // !core
} // !ux3d
