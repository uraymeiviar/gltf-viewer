// A3.2: sun shadow caster pass. Depth only: the cascade's light-space depth comes from the rasterizer,
// this shader only removes the fragments the forward pass removes. MASK uses the material's alpha cutoff;
// A1.7 dithered materials keep the Bayer pattern of asoboDitherDiscard, here in shadow-map texels (the
// shadow pass draws with u_AsoboDitherFrame 0, so the pattern is fixed and the filter turns it into
// partial shadow close to the material's alpha). OPAQUE draws never discard.

#include <environment_block.glsl>
#include <textures.glsl>
#include <functions.glsl>
#include <asobo_detail_map.glsl>
#include <asobo_windshield.glsl>
#include <asobo_dither.glsl>
#include <material_info.glsl>

void main()
{
#if ALPHAMODE == ALPHAMODE_MASK || defined(ASOBO_ALPHA_DITHER)
    vec4 baseColor = getBaseColor();
#endif

#if ALPHAMODE == ALPHAMODE_MASK
    if (baseColor.a < u_AlphaCutoff)
    {
        discard;
    }
#endif
#ifdef ASOBO_ALPHA_DITHER
    if (asoboDitherDiscard(baseColor.a))
    {
        discard;
    }
#endif
}
