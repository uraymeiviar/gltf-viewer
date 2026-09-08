// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "core/Result.h"
#include "renderer/gl/GL.h"

#include <limits>

namespace ux3d::renderer::gl {
static constexpr uint32_t maxActiveSamplers = GL_ACTIVE_TEXTURE - GL_TEXTURE0;
static constexpr GLuint invalid = std::numeric_limits<GLuint>::max();

enum class PrimitiveType : GLenum {
    Invalid = invalid,
    Bool = GL_BOOL,
    BoolVec2 = GL_BOOL_VEC2,
    BoolVec3 = GL_BOOL_VEC3,
    BoolVec4 = GL_BOOL_VEC4,
    UnsignedByte = GL_UNSIGNED_BYTE,
    UnsignedShort = GL_UNSIGNED_SHORT,
    Int = GL_INT,
    IntVec2 = GL_INT_VEC2,
    IntVec3 = GL_INT_VEC3,
    IntVec4 = GL_INT_VEC4,
    UnsignedInt = GL_UNSIGNED_INT,
    UnsignedIntVec2 = GL_UNSIGNED_INT_VEC2,
    UnsignedIntVec3 = GL_UNSIGNED_INT_VEC3,
    UnsignedIntVec4 = GL_UNSIGNED_INT_VEC4,
    Float = GL_FLOAT,
    FloatVec2 = GL_FLOAT_VEC2,
    FloatVec3 = GL_FLOAT_VEC3,
    FloatVec4 = GL_FLOAT_VEC4,
    FloatMat2 = GL_FLOAT_MAT2,
    FloatMat3 = GL_FLOAT_MAT3,
    FloatMat4 = GL_FLOAT_MAT4,
    Sampler1D = GL_SAMPLER_1D,
    Sampler2D = GL_SAMPLER_2D,
    SamplerArray2D = GL_SAMPLER_2D_ARRAY,
    SamplerArray1D = GL_SAMPLER_1D_ARRAY,
    Sampler3D = GL_SAMPLER_3D,
    SamplerCube = GL_SAMPLER_CUBE
};

enum class BufferTarget : GLenum {
    None = 0,
    ArrayBuffer = GL_ARRAY_BUFFER,
    ElementArrayBuffer = GL_ELEMENT_ARRAY_BUFFER,
    UniformBuffer = GL_UNIFORM_BUFFER
};

enum class TextureTarget : GLenum {
    Invalid = GL_INVALID_ENUM,
    Texture2d = GL_TEXTURE_2D,
    Texture2dMultisample = GL_TEXTURE_2D_MULTISAMPLE,
    Texture3d = GL_TEXTURE_3D,
    Texture2dArray = GL_TEXTURE_2D_ARRAY,
    TextureCubeMap = GL_TEXTURE_CUBE_MAP,

    TextureCubeMapPositiveX = GL_TEXTURE_CUBE_MAP_POSITIVE_X,
    TextureCubeMapNegativeX = GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
    TextureCubeMapPositiveY = GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
    TextureCubeMapNegativeY = GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
    TextureCubeMapPositiveZ = GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
    TextureCubeMapNegativeZ = GL_TEXTURE_CUBE_MAP_NEGATIVE_Z
};

enum class Format : GLenum {
    R = GL_RED,
    RInt = GL_RED_INTEGER,
    RG = GL_RG,
    RGB = GL_RGB,
    BGR = GL_BGR,
    RGBA = GL_RGBA,
    BGRA = GL_BGRA,
    D = GL_DEPTH_COMPONENT,
    DS = GL_DEPTH_STENCIL
};

enum class InternalFormat : GLint {
    Identity = 0x0,

    R8 = GL_R8,
    SR8 = GL_R8_SNORM, /* GL_VERSION_3_0 */
    R8Ui = GL_R8UI,

    R16 = GL_R16,
    R16Ui = GL_R16UI,

    R32Ui = GL_R32UI,
    R32F = GL_R32F,

    RG8 = GL_RG8,
    SRG8 = GL_RG8_SNORM, /* GL_VERSION_3_0 */
    RG16 = GL_RG16,

    D24S8 = GL_DEPTH24_STENCIL8,

    SRGB8 = GL_SRGB8,
    RGB8 = GL_RGB8,
    RGB10 = GL_RGB10,

    SRGB8_A8 = GL_SRGB8_ALPHA8,
    RGBA8 = GL_RGBA8,

    R16F = GL_R16F,
    RG16F = GL_RG16F,
    RGB16F = GL_RGB16F,
    RGBA16F = GL_RGBA16F,

    RG32F = GL_RG32F,
    RGB32F = GL_RGB32F,
    RGBA32F = GL_RGBA32F,
};

enum class ComponentType : GLenum {
    Unknown = 0,
    Byte = GL_BYTE,
    UByte = GL_UNSIGNED_BYTE,
    Short = GL_SHORT,
    UShort = GL_UNSIGNED_SHORT,
    UInt = GL_UNSIGNED_INT,
    HalfFloat = GL_HALF_FLOAT,
    Float = GL_FLOAT,
    UInt24D8S = GL_UNSIGNED_INT_24_8,
};

enum class TextureMinFilter : GLenum {
    Nearest = GL_NEAREST,
    Linear = GL_LINEAR,
    NearestMipmapNearest = GL_NEAREST_MIPMAP_NEAREST,
    NearestMipmapLinear = GL_NEAREST_MIPMAP_LINEAR,
    LinearMipmapNearest = GL_LINEAR_MIPMAP_NEAREST,
    LinearMipmapLinear = GL_LINEAR_MIPMAP_LINEAR,
};

enum class TextureMagFilter : GLenum {
    Nearest = GL_NEAREST,
    Linear = GL_LINEAR,
};

enum class TextureWrap : GLenum {
    ClampToEdge = GL_CLAMP_TO_EDGE,
    MirroredRepeat = GL_MIRRORED_REPEAT,
    Repeat = GL_REPEAT,
};

enum class Culling : GLenum {
    None,
    Front = GL_FRONT,
    Back = GL_BACK,
    FrontAndBack = GL_FRONT_AND_BACK
};

enum class Winding : GLenum {
    Clockwise = GL_CW,
    CounterClockwise = GL_CCW
};

enum class PolygonMode : GLenum {
    Fill = GL_FILL,
    Line = GL_LINE,
    Point = GL_POINT
};

enum class PrimitiveTopology : GLenum {
    TriangleList = GL_TRIANGLES,
    TriangleStrip = GL_TRIANGLE_STRIP,
    LineList = GL_LINES,
    LineStrip = GL_LINE_STRIP,
    Points = GL_POINTS,
};

enum class ClearTarget : GLbitfield {
    ColorBuffer = GL_COLOR_BUFFER_BIT,
    DepthBuffer = GL_DEPTH_BUFFER_BIT,
    All = ColorBuffer | DepthBuffer
};

enum class Attachment : GLenum {
    Color0 = GL_COLOR_ATTACHMENT0,
    Color1 = GL_COLOR_ATTACHMENT1,
    Color2 = GL_COLOR_ATTACHMENT2,
    Color3 = GL_COLOR_ATTACHMENT3,
    Depth = GL_DEPTH_ATTACHMENT,
};
}
