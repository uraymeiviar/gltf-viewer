// Per-frame environment and exposure values, uploaded once per scene draw (P8.1c). The
// binding point is set by gl::Program (kEnvironmentBlockBinding). Member names match the
// former uniforms.
layout(std140) uniform EnvironmentBlock
{
    mat3 u_EnvRotation;
    float u_EnvIntensity;
    float u_EnvBlurNormalized;
    int u_MipCount;
    float u_Exposure;
    // Non-zero when this asset declares ASOBO_normal_map_convention "DirectX" and the
    // viewport applies it (A1.2). It is per viewport, so it cannot be a material define.
    int u_AsoboNormalMapDirectX;
    // Non-zero when the viewport applies ASOBO_material_windshield_v3. Materials that
    // declare it then use COLOR_0 as masks, not as a base-colour multiplier (A4 partial).
    int u_AsoboWindshieldVertexColorMask;
    // Non-zero when this viewport has a filled decal buffer (A2.2). Only materials compiled
    // with ASOBO_DECAL_RECEIVER read A0-A5, and this per-viewport switch keeps the command
    // cache independent of which viewport draws them.
    int u_AsoboDecalReceiver;
    float u_DecalDepthTolerance; // metres along the receiver geometric normal (A2.2)

    // A1.6 / A2.2 MSFS emissive pipeline
    int u_AsoboEmissiveActive;
    float u_MsfsEmissiveScale;
    int u_TimeOfDay;
    int u_AsoboMaterialEmissiveApplied;
    // Non-zero when the viewport applies ASOBO_material_detail_map (A5.1). Materials that
    // declare the extension compile its variant, and this switch decides whether it is
    // sampled, so the command cache stays independent of the viewport.
    int u_AsoboDetailMapEnabled;
    // Windshield icing factor 0..1 (A4.2). Coverage = detailColorTexture.g * vertexColor.a * u_WindshieldIcing.
    float u_WindshieldIcing;
    // Non-zero when the viewport applies ASOBO_occlusion_strength (A1.5); materials that
    // declare it compile ASOBO_OCCLUSION_STRENGTH and read u_AsoboOcclusionStrength.
    int u_AsoboOcclusionStrengthApplied;
    int u_AsoboUVOptionsEnabled; // A5.2, per-viewport main UV transform/clamp
    // Non-zero when the viewport applies ASOBO_material_fresnel_fade / ASOBO_material_ghost_effect (A4.4);
    // BLEND materials that declare them compile ASOBO_FRESNEL_FADE / ASOBO_GHOST_EFFECT.
    int u_AsoboFresnelFadeApplied;
    int u_AsoboGhostEffectApplied;
    // Non-zero when the viewport applies ASOBO_material_alphamode_dither (A1.7); materials that declare
    // it (or the core "DITHER" string) compile ASOBO_ALPHA_DITHER. u_AsoboDitherFrame advances the
    // pattern per TAA accumulation frame and is 0 without TAA.
    int u_AsoboAlphaDitherApplied;
    float u_AsoboDitherFrame;

    // A3.1 sun: a directional light, scene-level (docs/ASOBO_EXTENSIONS_PHASE_3.md, A3.1).
    // u_SunDirection: world space, unit, from the surface towards the sun. u_SunIlluminance: linear RGB
    // illuminance on a surface facing the sun, in the environment's radiance units x sr (a white Lambert
    // surface facing it reflects u_SunIlluminance / pi). u_SunAngularRadius: radians.
    vec3 u_SunDirection;
    int u_SunEnabled;
    vec3 u_SunIlluminance;
    float u_SunAngularRadius;

    // A3.2 cascaded sun shadow maps (shadow.glsl; docs/ASOBO_EXTENSIONS_PHASE_3.md, A3.2). Per viewport:
    // u_ShadowMatrices[i] maps a world position to cascade i's shadow-map texture space (xy in 0..1, z the
    // window depth 0..1). u_ShadowSplits[i]: the view depth (metres in front of the camera) where cascade i
    // ends. u_ShadowTexelSizes[i]: world size of one shadow-map texel of cascade i.
    // u_ShadowParams: x = view depth where cascade 0 starts, y/z = view depth where the shadow starts / ends
    // fading out (only when the max distance cuts the scene), w = normal offset in texels.
    // u_ShadowEnabled is 0 whenever the sun or its shadows are off: the sun term is then unshadowed.
    mat4 u_ShadowMatrices[4];
    vec4 u_ShadowSplits;
    vec4 u_ShadowTexelSizes;
    vec4 u_ShadowParams;
    int u_ShadowEnabled;
    int u_ShadowCascadeCount;
    int u_ShadowFilter; // 0 hard (one bilinear compare), 1 PCF (3x3 bilinear compares), 2 soft (rotated Poisson)
    float u_ShadowMapSize; // texels per side

    // A3.5 screen-space ambient occlusion (ssao.glsl): non-zero while this viewport computed the SSAO term this
    // frame; pbr.frag then attenuates the IBL terms of OPAQUE / MASK surfaces with it.
    int u_SsaoEnabled;

    // A3.7 ASOBO_sky_portal (asobo_sky_portal.glsl): the viewport's portals, read only by materials compiled with
    // ASOBO_SKY_PORTAL. u_SkyPortalPositionRadius[i]: xyz world centre, w disc radius (world units);
    // u_SkyPortalNormalCosInner[i]: xyz unit normal into the cabin, w cos(inner cone angle / 2);
    // u_SkyPortalCosOuter[i].x: cos(outer cone angle / 2). u_SkyPortalStrength scales the added light.
    int u_SkyPortalCount;
    float u_SkyPortalStrength;
    vec4 u_SkyPortalPositionRadius[8];
    vec4 u_SkyPortalNormalCosInner[8];
    vec4 u_SkyPortalCosOuter[8];

    // A6.4 pre-exposure (NightSky.h): the scene's linear outputs (pbr.frag, ground.frag, the procedural sky) are written
    // multiplied by u_PreExposure so night radiance (1e-8 .. 1e-5 environment units) stays inside the half-float scene
    // target; the post-processing exposure divides it out again. Exactly 1 except with the procedural sky in the Auto
    // or Curve exposure mode. Readers of the scene colour (transmission, SSR) divide by it.
    float u_PreExposure;
    // A6.5: night weight of the ASOBO_material_emissive multipliers while u_TimeOfDay is 2 (Automatic, twilight).
    float u_EmissiveNightBlend;
    // A3.8 viewer ambient light (SceneEnvironment.h kAmbientUnitLuminance): a uniform radiance from every direction in
    // environment units (before u_PreExposure), added to the IBL terms of pbr.frag and ground.frag while
    // u_AmbientEnabled is non-zero. Not part of the glTF; never in the IBL filter input or the background.
    vec3 u_AmbientRadiance;
    int u_AmbientEnabled;
    // A3.9 sky occlusion (sky_occlusion.glsl): the sky-visibility volume of this viewport's model. Origin: xyz world
    // position of the volume's minimum corner, w cell edge; Dims: xyz cell counts, w normal offset of the lookup in
    // cells; Params: x = 1 / the open fraction (values at or above it read fully open), y = 1 while the volume is used.
    // Read only by materials compiled with SKY_OCCLUSION.
    vec4 u_SkyOcclusionOrigin;
    vec4 u_SkyOcclusionDims;
    vec4 u_SkyOcclusionParams;
};
