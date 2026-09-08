// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

namespace ux3d {
namespace core {
#if !defined(UX3D_CPP17_SUPPORT) && (defined(__cplusplus) || defined(_MSVC_LANG))
#if (__cplusplus >= 201703L || _MSVC_LANG >= 201703L)
#define UX3D_CPP17_SUPPORT 1
#endif
#endif

#ifndef UX3D_CPP17_SUPPORT
#error "Compiler does not support C++ 17"
#endif
}
} // !ux3d
