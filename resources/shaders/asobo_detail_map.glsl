// A5.1: ASOBO_material_detail_map (docs/ASOBO_EXTENSIONS_SEMANTICS.md §14). Detail colour,
// metal-rough-AO and normal layers tile TEXCOORD_0 by UVScale and blend over the base
// material, either through the vertex alpha (detail mode) or through blendMaskTexture.r
// with blendThreshold (blend mode). The extension is a material-level define; whether the
// viewport applies it is u_AsoboDetailMapEnabled, so the command cache does not depend on
// the viewport.
#ifdef ASOBO_MATERIAL_DETAIL_MAP

uniform float u_AsoboDetailUVScale;
uniform vec2 u_AsoboDetailUVOffset;
uniform float u_AsoboDetailNormalScale;
uniform float u_AsoboDetailBlendThreshold;
uniform int u_DetailColorUVSet;
uniform int u_DetailOrmUVSet;
uniform int u_DetailNormalUVSet;
uniform int u_DetailBlendMaskUVSet;

#ifdef HAS_DETAIL_COLOR_MAP
uniform sampler2D u_DetailColorSampler;
#endif
#ifdef HAS_DETAIL_ORM_MAP
uniform sampler2D u_DetailOrmSampler;
#endif
#ifdef HAS_DETAIL_NORMAL_MAP
uniform sampler2D u_DetailNormalSampler;
#endif
#ifdef ASOBO_DETAIL_BLEND_MASK
uniform sampler2D u_DetailBlendMaskSampler;
#endif


// Transformed main UV times UVScale, then the offset. The exporter also multiplies an undocumented
// object attribute "scale" (§14 open question 1); Glider ignores it.
vec2 getAsoboDetailUV(int uvSet, sampler2D map)
{
    vec2 uv = uvSet == 0 ? getAsoboMainUV(v_texcoord_0) : v_texcoord_1;
    return clampAsoboUV(uv * u_AsoboDetailUVScale + u_AsoboDetailUVOffset, uvSet, map);
}


// The exporter's shared "Mask" (§14): the vertex alpha in detail mode, 1 - coverage in blend
// mode, with coverage = saturate((vA - low) / (high - low)), low = clamp(m - t, 0, 1) and
// high = clamp(m + t, 0, 1) for the mask red channel m on the main UV. It is returned as the
// detail weight: 0 keeps the base bytes and 1 applies the detail layer fully. A viewport
// with the extension switched off gets 0, so every operator below is an exact identity.
float getAsoboDetailWeight()
{
    if (u_AsoboDetailMapEnabled == 0) {
        return 0.0;
    }
    float vA = getVertexColor().a;
#ifdef ASOBO_DETAIL_BLEND_MASK
    vec2 uv = u_DetailBlendMaskUVSet == 0 ? getAsoboMainUV(v_texcoord_0) : v_texcoord_1;
    float m = texture(u_DetailBlendMaskSampler,
                      clampAsoboUV(uv, u_DetailBlendMaskUVSet, u_DetailBlendMaskSampler)).r;
    float t = max(u_AsoboDetailBlendThreshold, 1.0e-5);
    float low = clamp(m - t, 0.0, 1.0);
    float high = clamp(m + t, 0.0, 1.0);
    return 1.0 - clamp((vA - low) / max(high - low, 1.0e-5), 0.0, 1.0);
#else
    return vA;
#endif
}


// §14 detail colour. Detail mode mixes the base to smoothstep(clamp(sqrt(B * D)), 0, 1) by
// aD * vA, and the detail texture's alpha multiplies the material alpha. Blend mode mixes the
// detail colour to the base by the coverage. A zero weight returns the base bytes unchanged.
vec4 applyAsoboDetailColor(vec4 baseColor)
{
#ifdef HAS_DETAIL_COLOR_MAP
    if (u_AsoboDetailMapEnabled == 0) {
        return baseColor;
    }
    vec4 detail = texture(u_DetailColorSampler, getAsoboDetailUV(u_DetailColorUVSet, u_DetailColorSampler));
    float weight = getAsoboDetailWeight();
    vec3 target = detail.rgb;
#ifndef ASOBO_DETAIL_BLEND_MASK
    target = smoothstep(vec3(0.0), vec3(1.0), sqrt(max(baseColor.rgb * detail.rgb, vec3(0.0))));
    weight *= detail.a;
#endif
    baseColor.rgb = mix(baseColor.rgb, target, weight);
    baseColor.a *= detail.a;
#endif
    return baseColor;
}


#ifdef HAS_DETAIL_ORM_MAP
// The detail OMR texel. The packaged A1.12 convention stores the COMP red channel as
// sqrt(linear AO), the same gating the base occlusion map uses.
vec3 getAsoboDetailOrm()
{
    vec3 orm = texture(u_DetailOrmSampler, getAsoboDetailUV(u_DetailOrmUVSet, u_DetailOrmSampler)).rgb;
#ifdef ASOBO_ASSET_OPTIMIZED
    orm.r *= orm.r;
#endif
    return orm;
}
#endif


// §14 detail ORM. Detail mode adds the unpacked detail offsets to the factor-scaled base
// values (the exporter graph scales the base OMR sample and adds the offset unscaled,
// blender/material/msfs_material.py:2109-2121). Blend mode replaces the channel with
// mix(detail, base, coverage). A zero weight returns the base values unchanged.
void applyAsoboDetailOrm(float weight, float metallicScale, float roughnessScale,
                         inout float metallic, inout float perceptualRoughness)
{
#ifdef HAS_DETAIL_ORM_MAP
    if (weight <= 0.0) {
        return;
    }
    vec3 orm = getAsoboDetailOrm();
#ifdef ASOBO_DETAIL_BLEND_MASK
    metallic = mix(metallic, orm.b * metallicScale, weight);
    perceptualRoughness = mix(perceptualRoughness, orm.g * roughnessScale, weight);
#else
    vec3 offset = (2.0 * orm - 1.0) * weight;
    metallic = clamp(metallic + offset.b, 0.0, 1.0);
    perceptualRoughness = clamp(perceptualRoughness + offset.g, 0.0, 1.0);
#endif
#endif
}


// §14 detail AO. Detail mode adds the unpacked red channel; blend mode mixes the detail AO
// to the base by the coverage. A zero weight returns the base occlusion unchanged.
float applyAsoboDetailOcclusion(float weight, float ao)
{
#ifdef HAS_DETAIL_ORM_MAP
    if (weight <= 0.0) {
        return ao;
    }
    vec3 orm = getAsoboDetailOrm();
#ifdef ASOBO_DETAIL_BLEND_MASK
    return mix(ao, orm.r, weight);
#else
    return clamp(ao + (2.0 * orm.r - 1.0) * weight, 0.0, 1.0);
#endif
#else
    return ao;
#endif
}


// §14 detail normal. The map is decoded exactly like the base normal (A1.2 DirectX flip and
// the A1.12 packaged decode, with the same per-texture RG gate as the base map) and scaled by
// detailNormalTexture.scale. Detail mode combines it with the base tangent-space normal by
// the normalised whiteout operator; blend mode lerps the detail normal to the base by the
// coverage, as the exporter's graph does. A zero weight returns the base bytes unchanged.
vec3 applyAsoboDetailNormal(vec3 baseTangentNormal)
{
#ifdef HAS_DETAIL_NORMAL_MAP
    float weight = getAsoboDetailWeight();
    if (weight <= 0.0) {
        return baseTangentNormal;
    }
#ifdef DETAIL_NORMAL_TEXTURE_RG_SNORM
    vec3 detail = sampleTangentSpaceNormalRG(u_DetailNormalSampler, getAsoboDetailUV(u_DetailNormalUVSet, u_DetailNormalSampler));
#else
    vec3 detail = sampleTangentSpaceNormal(u_DetailNormalSampler, getAsoboDetailUV(u_DetailNormalUVSet, u_DetailNormalSampler));
#endif
    detail *= vec3(u_AsoboDetailNormalScale, u_AsoboDetailNormalScale, 1.0);
    detail = normalize(detail);
#ifdef ASOBO_DETAIL_BLEND_MASK
    return mix(baseTangentNormal, detail, weight);
#else
    vec3 whiteout = mix(vec3(0.0, 0.0, 1.0), detail, weight);
    return vec3(baseTangentNormal.xy + whiteout.xy, baseTangentNormal.z * whiteout.z);
#endif
#else
    return baseTangentNormal;
#endif
}

#endif // ASOBO_MATERIAL_DETAIL_MAP
