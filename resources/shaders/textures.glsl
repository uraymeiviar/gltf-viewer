// IBL


uniform samplerCube u_LambertianEnvSampler;
uniform samplerCube u_GGXEnvSampler;
uniform sampler2D u_GGXLUT;
uniform samplerCube u_CharlieEnvSampler;
uniform sampler2D u_CharlieLUT;
uniform sampler2D u_SheenELUT;


// General Material


uniform sampler2D u_NormalSampler;
uniform float u_NormalScale;
uniform int u_NormalUVSet;
uniform mat3 u_NormalUVTransform;

uniform vec3 u_EmissiveFactor;
uniform sampler2D u_EmissiveSampler;
uniform int u_EmissiveUVSet;
uniform mat3 u_EmissiveUVTransform;

uniform sampler2D u_OcclusionSampler;
uniform int u_OcclusionUVSet;
uniform float u_OcclusionStrength;
#ifdef ASOBO_OCCLUSION_STRENGTH
uniform float u_AsoboOcclusionStrength;
#endif
uniform mat3 u_OcclusionUVTransform;

// A4.4: view-angle opacity of ASOBO_material_fresnel_fade (x = fresnelFactor, y = fresnelOpacityOffset)
// and ASOBO_material_ghost_effect (x = bias, y = scale, z = power); BLEND materials only.
#ifdef ASOBO_FRESNEL_FADE
uniform vec2 u_AsoboFresnelFade;
#endif
#ifdef ASOBO_GHOST_EFFECT
uniform vec3 u_AsoboGhost;
#endif


in vec2 v_texcoord_0;
in vec2 v_texcoord_1;

// A5.2: material main-UV transform, before each texture's KHR_texture_transform.
// The SDK leaves order/sign/pivot unspecified; see semantics section 15.
#ifdef ASOBO_MATERIAL_UV_OPTIONS
uniform vec2 u_AsoboUVTiling;
uniform vec2 u_AsoboUVOffset;
uniform vec2 u_AsoboUVRotation; // cos(theta), sin(theta), theta in degrees in the asset
uniform vec2 u_AsoboUVClamp;
#endif

vec2 getAsoboMainUV(vec2 uv)
{
#ifdef ASOBO_MATERIAL_UV_OPTIONS
    if (u_AsoboUVOptionsEnabled != 0) {
        vec2 p = (uv - 0.5) * u_AsoboUVTiling;
        vec2 r = u_AsoboUVRotation;
        return mat2(r.x, r.y, -r.y, r.x) * p + 0.5 + u_AsoboUVOffset;
    }
#endif
    return uv;
}

// Clamp after all coordinate transforms, independently per axis. Half a base-level
// texel keeps repeat samplers from wrapping at exactly 0/1 (also with linear filtering).
vec2 clampAsoboUV(vec2 uv, int uvSet, sampler2D map)
{
#ifdef ASOBO_MATERIAL_UV_OPTIONS
    if (u_AsoboUVOptionsEnabled != 0 && uvSet == 0) {
        vec2 edge = 0.5 / vec2(textureSize(map, 0));
        if (u_AsoboUVClamp.x > 0.5) uv.x = clamp(uv.x, edge.x, 1.0 - edge.x);
        if (u_AsoboUVClamp.y > 0.5) uv.y = clamp(uv.y, edge.y, 1.0 - edge.y);
    }
#endif
    return uv;
}


vec2 getNormalUV()
{
    vec3 uv = vec3(u_NormalUVSet < 1 ? getAsoboMainUV(v_texcoord_0) : v_texcoord_1, 1.0);

#ifdef HAS_NORMAL_UV_TRANSFORM
    uv = u_NormalUVTransform * uv;
#endif

    return clampAsoboUV(uv.xy, u_NormalUVSet, u_NormalSampler);
}


vec2 getEmissiveUV()
{
    vec3 uv = vec3(u_EmissiveUVSet < 1 ? getAsoboMainUV(v_texcoord_0) : v_texcoord_1, 1.0);

#ifdef HAS_EMISSIVE_UV_TRANSFORM
    uv = u_EmissiveUVTransform * uv;
#endif

    return clampAsoboUV(uv.xy, u_EmissiveUVSet, u_EmissiveSampler);
}


vec2 getOcclusionUV()
{
    vec3 uv = vec3(u_OcclusionUVSet < 1 ? getAsoboMainUV(v_texcoord_0) : v_texcoord_1, 1.0);

#ifdef HAS_OCCLUSION_UV_TRANSFORM
    uv = u_OcclusionUVTransform * uv;
#endif

    return clampAsoboUV(uv.xy, u_OcclusionUVSet, u_OcclusionSampler);
}


// Metallic Roughness Material


#ifdef MATERIAL_METALLICROUGHNESS

uniform sampler2D u_BaseColorSampler;
uniform int u_BaseColorUVSet;
uniform mat3 u_BaseColorUVTransform;

uniform sampler2D u_MetallicRoughnessSampler;
uniform int u_MetallicRoughnessUVSet;
uniform mat3 u_MetallicRoughnessUVTransform;

vec2 getBaseColorUV()
{
    vec3 uv = vec3(u_BaseColorUVSet < 1 ? getAsoboMainUV(v_texcoord_0) : v_texcoord_1, 1.0);

#ifdef HAS_BASECOLOR_UV_TRANSFORM
    uv = u_BaseColorUVTransform * uv;
#endif

    return clampAsoboUV(uv.xy, u_BaseColorUVSet, u_BaseColorSampler);
}

vec2 getMetallicRoughnessUV()
{
    vec3 uv = vec3(u_MetallicRoughnessUVSet < 1 ? getAsoboMainUV(v_texcoord_0) : v_texcoord_1, 1.0);

#ifdef HAS_METALLICROUGHNESS_UV_TRANSFORM
    uv = u_MetallicRoughnessUVTransform * uv;
#endif

    return clampAsoboUV(uv.xy, u_MetallicRoughnessUVSet, u_MetallicRoughnessSampler);
}

#endif


// Specular Glossiness Material


#ifdef MATERIAL_SPECULARGLOSSINESS

uniform sampler2D u_DiffuseSampler;
uniform int u_DiffuseUVSet;
uniform mat3 u_DiffuseUVTransform;

uniform sampler2D u_SpecularGlossinessSampler;
uniform int u_SpecularGlossinessUVSet;
uniform mat3 u_SpecularGlossinessUVTransform;


vec2 getSpecularGlossinessUV()
{
    vec3 uv = vec3(u_SpecularGlossinessUVSet < 1 ? v_texcoord_0 : v_texcoord_1, 1.0);

#ifdef HAS_SPECULARGLOSSINESS_UV_TRANSFORM
    uv = u_SpecularGlossinessUVTransform * uv;
#endif

    return uv.xy;
}

vec2 getDiffuseUV()
{
    vec3 uv = vec3(u_DiffuseUVSet < 1 ? v_texcoord_0 : v_texcoord_1, 1.0);

#ifdef HAS_DIFFUSE_UV_TRANSFORM
    uv = u_DiffuseUVTransform * uv;
#endif

    return uv.xy;
}

#endif


// Clearcoat Material


#ifdef MATERIAL_CLEARCOAT

uniform sampler2D u_ClearcoatSampler;
uniform int u_ClearcoatUVSet;
uniform mat3 u_ClearcoatUVTransform;

uniform sampler2D u_ClearcoatRoughnessSampler;
uniform int u_ClearcoatRoughnessUVSet;
uniform mat3 u_ClearcoatRoughnessUVTransform;

uniform sampler2D u_ClearcoatNormalSampler;
uniform int u_ClearcoatNormalUVSet;
uniform mat3 u_ClearcoatNormalUVTransform;
uniform float u_ClearcoatNormalScale;


vec2 getClearcoatUV()
{
    vec3 uv = vec3(u_ClearcoatUVSet < 1 ? v_texcoord_0 : v_texcoord_1, 1.0);
#ifdef HAS_CLEARCOAT_UV_TRANSFORM
    uv = u_ClearcoatUVTransform * uv;
#endif
    return uv.xy;
}

vec2 getClearcoatRoughnessUV()
{
    vec3 uv = vec3(u_ClearcoatRoughnessUVSet < 1 ? v_texcoord_0 : v_texcoord_1, 1.0);
#ifdef HAS_CLEARCOATROUGHNESS_UV_TRANSFORM
    uv = u_ClearcoatRoughnessUVTransform * uv;
#endif
    return uv.xy;
}

vec2 getClearcoatNormalUV()
{
    vec3 uv = vec3(u_ClearcoatNormalUVSet < 1 ? v_texcoord_0 : v_texcoord_1, 1.0);
#ifdef HAS_CLEARCOATNORMAL_UV_TRANSFORM
    uv = u_ClearcoatNormalUVTransform * uv;
#endif
    return uv.xy;
}

#endif


// Sheen Material


#ifdef MATERIAL_SHEEN

uniform sampler2D u_SheenColorSampler;
uniform int u_SheenColorUVSet;
uniform mat3 u_SheenColorUVTransform;
uniform sampler2D u_SheenRoughnessSampler;
uniform int u_SheenRoughnessUVSet;
uniform mat3 u_SheenRoughnessUVTransform;


vec2 getSheenColorUV()
{
    vec3 uv = vec3(u_SheenColorUVSet < 1 ? v_texcoord_0 : v_texcoord_1, 1.0);
#ifdef HAS_SHEENCOLOR_UV_TRANSFORM
    uv = u_SheenColorUVTransform * uv;
#endif
    return uv.xy;
}

vec2 getSheenRoughnessUV()
{
    vec3 uv = vec3(u_SheenRoughnessUVSet < 1 ? v_texcoord_0 : v_texcoord_1, 1.0);
#ifdef HAS_SHEENROUGHNESS_UV_TRANSFORM
    uv = u_SheenRoughnessUVTransform * uv;
#endif
    return uv.xy;
}

#endif


// Specular Material


#ifdef MATERIAL_SPECULAR

uniform sampler2D u_SpecularSampler;
uniform int u_SpecularUVSet;
uniform mat3 u_SpecularUVTransform;
uniform sampler2D u_SpecularColorSampler;
uniform int u_SpecularColorUVSet;
uniform mat3 u_SpecularColorUVTransform;


vec2 getSpecularUV()
{
    vec3 uv = vec3(u_SpecularUVSet < 1 ? v_texcoord_0 : v_texcoord_1, 1.0);
#ifdef HAS_SPECULAR_UV_TRANSFORM
    uv = u_SpecularUVTransform * uv;
#endif
    return uv.xy;
}

vec2 getSpecularColorUV()
{
    vec3 uv = vec3(u_SpecularColorUVSet < 1 ? v_texcoord_0 : v_texcoord_1, 1.0);
#ifdef HAS_SPECULARCOLOR_UV_TRANSFORM
    uv = u_SpecularColorUVTransform * uv;
#endif
    return uv.xy;
}

#endif


// Transmission Material


#ifdef MATERIAL_TRANSMISSION

uniform sampler2D u_TransmissionSampler;
uniform int u_TransmissionUVSet;
uniform mat3 u_TransmissionUVTransform;



vec2 getTransmissionUV()
{
    vec3 uv = vec3(u_TransmissionUVSet < 1 ? v_texcoord_0 : v_texcoord_1, 1.0);
#ifdef HAS_TRANSMISSION_UV_TRANSFORM
    uv = u_TransmissionUVTransform * uv;
#endif
    return uv.xy;
}

#endif


// Volume Material


#ifdef MATERIAL_VOLUME

uniform sampler2D u_ThicknessSampler;
uniform int u_ThicknessUVSet;
uniform mat3 u_ThicknessUVTransform;


vec2 getThicknessUV()
{
    vec3 uv = vec3(u_ThicknessUVSet < 1 ? v_texcoord_0 : v_texcoord_1, 1.0);
#ifdef HAS_THICKNESS_UV_TRANSFORM
    uv = u_ThicknessUVTransform * uv;
#endif
    return uv.xy;
}

#endif


// Iridescence


#ifdef MATERIAL_IRIDESCENCE

uniform sampler2D u_IridescenceSampler;
uniform int u_IridescenceUVSet;
uniform mat3 u_IridescenceUVTransform;

uniform sampler2D u_IridescenceThicknessSampler;
uniform int u_IridescenceThicknessUVSet;
uniform mat3 u_IridescenceThicknessUVTransform;


vec2 getIridescenceUV()
{
    vec3 uv = vec3(u_IridescenceUVSet < 1 ? v_texcoord_0 : v_texcoord_1, 1.0);
#ifdef HAS_IRIDESCENCE_UV_TRANSFORM
    uv = u_IridescenceUVTransform * uv;
#endif
    return uv.xy;
}

vec2 getIridescenceThicknessUV()
{
    vec3 uv = vec3(u_IridescenceThicknessUVSet < 1 ? v_texcoord_0 : v_texcoord_1, 1.0);
#ifdef HAS_IRIDESCENCETHICKNESS_UV_TRANSFORM
    uv = u_IridescenceThicknessUVTransform * uv;
#endif
    return uv.xy;
}

#endif

#if defined(MATERIAL_TRANSMISSION) || defined(ASOBO_GLASS_REFRACTION) || defined(ASOBO_WINDSHIELD_SSR)
uniform sampler2D u_TransmissionFramebufferSampler;
uniform ivec2 u_TransmissionFramebufferSize;
#endif
