// Copyright (c) 2019 UX3D GmbH. All rights reserved.

//
// This fragment shader defines a reference implementation for Physically Based Shading of
// a microfacet surface material defined by a glTF model.
//
// References:
// [1] Real Shading in Unreal Engine 4
//     http://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf
// [2] Physically Based Shading at Disney
//     http://blog.selfshadow.com/publications/s2012-shading-course/burley/s2012_pbs_disney_brdf_notes_v3.pdf
// [3] README.md - Environment Maps
//     https://github.com/KhronosGroup/glTF-WebGL-PBR/#environment-maps
// [4] "An Inexpensive BRDF Model for Physically based Rendering" by Christophe Schlick
//     https://www.cs.virginia.edu/~jdl/bib/appearance/analytic%20models/schlick94b.pdf

layout(location = 0) out vec4 g_pickingColor;

#include <environment_block.glsl>
#include <tonemapping.glsl>
#include <textures.glsl>
#include <functions.glsl>
#include <brdf.glsl>
#include <punctual.glsl>
#include <ibl.glsl>

uniform vec4 u_BaseColorFactor;
uniform vec4 u_DiffuseFactor;

uniform float u_AlphaCutoff = 0.5f;

uniform vec3 u_PickingColor = vec3(0.0);


vec4 getBaseColor()
{
    vec4 baseColor = vec4(1, 1, 1, 1);

#ifdef MATERIAL_SPECULARGLOSSINESS
    baseColor = u_DiffuseFactor;
    #ifdef HAS_DIFFUSE_MAP
        baseColor *= texture(u_DiffuseSampler, getDiffuseUV());
    #endif
#endif
#ifdef MATERIAL_METALLICROUGHNESS
    baseColor = u_BaseColorFactor;
    #ifdef HAS_BASE_COLOR_MAP
        baseColor *= texture(u_BaseColorSampler, getBaseColorUV());
    #endif
#endif

    return baseColor * getVertexColor();
}

void main()
{
    vec4 baseColor = getBaseColor();

    #if ALPHAMODE == ALPHAMODE_OPAQUE
    baseColor.a = 1.0;
    #endif

    #if ALPHAMODE == ALPHAMODE_MASK
    if(baseColor.a < u_AlphaCutoff)
    {
        discard;
    }
    baseColor.a = 1.0;
    #endif

    g_pickingColor = vec4(u_PickingColor, 1.0);
}