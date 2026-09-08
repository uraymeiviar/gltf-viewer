// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

// Platform

#ifdef _WIN32
#define UX3D_IS_WINDOWS 1
#endif

#ifdef __APPLE__
#define UX3D_IS_APPLE 1
#endif

#if defined(__linux__) || defined(__linux)
#define UX3D_IS_LINUX 1
#endif

#if defined(UX3D_IS_APPLE) || defined(UX3D_IS_LINUX) || defined(__unix) || defined(__unix__)
#define UX3D_IS_POSIX 1
#endif

#if !defined(UX3D_IS_WINDOWS) && !defined(UX3D_IS_POSIX) && !defined(UX3D_IS_LINUX) && !defined(UX3D_IS_APPLE)
#error "Unknown platform"
#endif

// Compiler

#ifdef __GNUC__
#define UX3D_IS_GCC 1
#elif __clang__
#define UX3D_IS_CLANG 1
#elif _MSC_VER
#define UX3D_IS_MSVC 1
#endif

#if !defined(UX3D_IS_CLANG) && !defined(UX3D_IS_MSVC) && !defined(UX3D_IS_GCC)
#error "Unknown compiler"
#endif

// CPU Architecture
// http://nadeausoftware.com/articles/2012/02/c_c_tip_how_detect_processor_type_using_compiler_predefined_macros
// https://stackoverflow.com/questions/37244202/detect-arm-64-in-preprocessor
// http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.dui0491c/BABJFEFG.html

#if defined(_M_X64) || defined(_M_AMD64) || defined(__x86_64__) || defined(__amd64__)
#define UX3D_IS_X64 1
#endif

#if defined(_M_IX86) || defined(__i386__) || defined(__i386) || defined(_X86_)
#define UX3D_IS_X86 1
#endif

#if defined(_M_ARM) || defined(__arm__)
#define UX3D_IS_ARM 1
#endif

#if defined(_M_ARM64) || defined(__aarch64__)
#define UX3D_IS_ARM64 1
#endif

#if !defined(UX3D_IS_X64) && !defined(UX3D_IS_X86) && !defined(UX3D_IS_ARM) && !defined(UX3D_IS_ARM64)
#error "Unknown CPU architecture"
#endif

// Windows-specific archtivecture switches
#ifdef UX3D_IS_MSVC
#if defined(UX3D_IS_X86) && !defined(_X86_)
#define _X86_
#endif
#if defined(UX3D_IS_X64) && !defined(_AMD64_)
#define _AMD64_
#endif
#if defined(UX3D_IS_ARM) && !defined(_ARM_)
#define _ARM_
#endif
#if defined(_M_ARM64) && !defined(_ARM64_)
#define _ARM64_
#endif
#endif

// Warning ignore switches

#if UX3D_IS_CLANG
#define UX3D_BEGIN_IGNORE_WARNINGS _Pragma("clang diagnostic push") _Pragma("clang diagnostic ignored \"-Weverything\"")
#define UX3D_END_IGNORE_WARNINGS _Pragma("clang diagnostic pop")
#elif UX3D_IS_MSVC
#define UX3D_BEGIN_IGNORE_WARNINGS _Pragma("warning(push, 0)")
#define UX3D_END_IGNORE_WARNINGS _Pragma("warning(pop)")
#elif UX3D_IS_GCC
#define UX3D_BEGIN_IGNORE_WARNINGS                                              \
    _Pragma("GCC diagnostic ignored \"-Wall\"")                                 \
        _Pragma("GCC diagnostic ignored \"-Wextra\"")                           \
            _Pragma("GCC diagnostic ignored \"-Wconversion\"")                  \
                _Pragma("GCC diagnostic ignored \"-Wfloat-conversion\"")        \
                    _Pragma("GCC diagnostic ignored \"-Wsign-conversion\"")     \
                        _Pragma("GCC diagnostic ignored \"-Wunused-variable\"") \
                            _Pragma("GCC diagnostic ignored \"-Wsign-compare\"")
#define UX3D_END_IGNORE_WARNINGS _Pragma("GCC diagnostic pop")
#endif
