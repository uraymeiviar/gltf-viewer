// A2.1: decal prepass. Draws OPAQUE and MASK receiver geometry into the shared decal
// buffer: depth and the world-space geometric normal, not the normal-mapped one. The
// alpha test is the forward shader's, so MASK coverage is identical.

layout(location = 0) out vec4 g_PrepassNormal;

#include <environment_block.glsl>
#include <textures.glsl>
#include <functions.glsl>
#include <asobo_detail_map.glsl>
#include <asobo_windshield.glsl>
#include <asobo_dither.glsl>
#include <material_info.glsl>

void main()
{
    vec4 baseColor = getBaseColor();

#if ALPHAMODE == ALPHAMODE_MASK
    if (baseColor.a < u_AlphaCutoff)
    {
        discard;
    }
#endif
#ifdef ASOBO_ALPHA_DITHER
    // A1.7: the forward pass's dither test at the same viewport pixel, so depth matches what is drawn.
    if (asoboDitherDiscard(baseColor.a))
    {
        discard;
    }
#endif

    // The same geometric normal the forward Geometry Normal debug view outputs.
#ifdef HAS_NORMAL_VEC3
#ifdef HAS_TANGENT_VEC4
    vec3 ng = normalize(v_TBN[2]);
#else
    vec3 ng = normalize(v_Normal);
#endif
#else
    vec3 ng = normalize(cross(dFdx(v_Position), dFdy(v_Position)));
#endif

    if (gl_FrontFacing == false)
    {
        ng *= -1.0;
    }

    g_PrepassNormal = vec4(ng * 0.5 + 0.5, 1.0);
}
