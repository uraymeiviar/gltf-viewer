// A4.2: Windshield layers for ASOBO_material_windshield_v3, ASOBO_material_windshield (v1)
// and extras.ASOBO_material_code: "Windshield".
// Layers:
// 1. Details 1 (scratches) and Details 2 (fingerprints) masks in detailColorTexture R & B
//    gated by vertex colour R & B, modulating roughness and lit surface scattering.
// 2. Secondary details (emissiveTexture on windshield; dirt/smudges), gated by vertex colour G,
//    blending into base colour and opacity (zero emission).
// 3. Detail reflection (detailMetalRoughAOTexture: R reflection, G roughness, B metallic).
// 4. Micro-scratches (scratchesNormalTexture), sampled at 3 rotations (0, 60, 120 deg)
//    aligned to U, scaled by microScratchesStrength.
// 5. Icing (coverage = detailColorTexture.g * vertexColor.a * u_WindshieldIcing) with icing
//    normal (detailNormalTexture) and frost appearance.

#ifdef ASOBO_MATERIAL_WINDSHIELD

uniform float u_WindshieldUVScale;
uniform float u_WindshieldDetailNormalScale;
uniform float u_WindshieldDetail1Rough;
uniform float u_WindshieldDetail1Opacity;
uniform float u_WindshieldDetail2Rough;
uniform float u_WindshieldDetail2Opacity;
uniform float u_MicroScratchesTiling;
uniform float u_MicroScratchesStrength;

#ifdef HAS_DETAIL_COLOR_MAP
uniform sampler2D u_DetailColorSampler;
#endif

#ifdef HAS_DETAIL_ORM_MAP
uniform sampler2D u_DetailOrmSampler;
#endif

#ifdef HAS_DETAIL_NORMAL_MAP
uniform sampler2D u_DetailNormalSampler;
#endif

#ifdef HAS_SCRATCHES_NORMAL_MAP
uniform sampler2D u_ScratchesNormalSampler;
#endif

vec2 getWindshieldDetailUV()
{
    return v_texcoord_0 * u_WindshieldUVScale;
}

// On windshields emissiveTexture is a secondary grime layer. Like the other
// windshield-only textures it is outside the main UV-options group (SDK / section 15).
vec2 getWindshieldSecondaryUV()
{
    vec3 uv = vec3(u_EmissiveUVSet < 1 ? v_texcoord_0 : v_texcoord_1, 1.0);
#ifdef HAS_EMISSIVE_UV_TRANSFORM
    uv = u_EmissiveUVTransform * uv;
#endif
    return uv.xy;
}

float getWindshieldIcingCoverage()
{
    if (u_WindshieldIcing <= 0.0) {
        return 0.0;
    }
#ifdef HAS_DETAIL_COLOR_MAP
    vec4 detailColor = texture(u_DetailColorSampler, getWindshieldDetailUV());
    float vertAlpha = getVertexColor().a;
    return clamp(detailColor.g * vertAlpha * u_WindshieldIcing, 0.0, 1.0);
#else
    return 0.0;
#endif
}

// A4.2.1: opacity controls the amount of light scattered by the surface marks.
// Raising the clear pane's alpha instead blends its dim diffuse colour over bright
// sky and makes dark marks. Keep clear-pane absorption and Fresnel/SSR independent.
// Inferred thin-layer approximation: calibrated gain, lit by the same diffuse radiance
// as the base colour. No mask boost, LOD bias, extra light source or emission.
vec3 getWindshieldDetailScattering(vec3 litBaseColor)
{
#ifdef HAS_DETAIL_COLOR_MAP
    vec4 detailColor = texture(u_DetailColorSampler, getWindshieldDetailUV());
    vec4 vertColor = getVertexColor();
    float coverage = clamp(detailColor.r * vertColor.r * u_WindshieldDetail1Opacity
                         + detailColor.b * vertColor.b * u_WindshieldDetail2Opacity, 0.0, 1.0);
    if (coverage > 0.0) {
        // SDK DA62: unit gain peaks at only 2/255 against the sky. A gain of 32
        // makes its subpixel scratches and low-valued fingerprints readable.
        const float kWindshieldDetailScatteringGain = 32.0;
        return litBaseColor * (coverage * kWindshieldDetailScatteringGain);
    }
#endif
    return vec3(0.0);
}

vec4 applyAsoboWindshieldBaseColor(vec4 baseColor)
{
    vec4 vertColor = getVertexColor();

#ifdef HAS_EMISSIVE_MAP
    vec4 secDetail = texture(u_EmissiveSampler, getWindshieldSecondaryUV());
    float secMask = vertColor.g * secDetail.a;
    baseColor.rgb = mix(baseColor.rgb, secDetail.rgb, secMask);
    baseColor.a = clamp(baseColor.a + secMask * 0.5, 0.0, 1.0);
#endif

    float icingCoverage = getWindshieldIcingCoverage();
    if (icingCoverage > 0.0) {
        vec3 frostColor = vec3(0.92, 0.94, 0.97);
        baseColor.rgb = mix(baseColor.rgb, frostColor, icingCoverage);
        baseColor.a = mix(baseColor.a, 0.85, icingCoverage);
    }

    return baseColor;
}

void applyAsoboWindshieldOrm(inout float metallic, inout float perceptualRoughness, inout float specularWeight)
{
    vec4 vertColor = getVertexColor();

#ifdef HAS_DETAIL_COLOR_MAP
    vec4 detailColor = texture(u_DetailColorSampler, getWindshieldDetailUV());
    float mask1 = detailColor.r * vertColor.r;
    float mask2 = detailColor.b * vertColor.b;
    perceptualRoughness = clamp(perceptualRoughness + mask1 * u_WindshieldDetail1Rough + mask2 * u_WindshieldDetail2Rough, 0.0, 1.0);
#endif

#ifdef HAS_EMISSIVE_MAP
    vec4 secDetail = texture(u_EmissiveSampler, getWindshieldSecondaryUV());
    float secMask = vertColor.g * secDetail.a;
    perceptualRoughness = clamp(perceptualRoughness + secMask * 0.3, 0.0, 1.0);
#endif

#ifdef HAS_DETAIL_ORM_MAP
    vec3 detOrm = texture(u_DetailOrmSampler, getWindshieldDetailUV()).rgb;
    specularWeight *= detOrm.r;
    perceptualRoughness = clamp(perceptualRoughness + (2.0 * detOrm.g - 1.0) * 0.5, 0.0, 1.0);
    metallic = clamp(metallic + (2.0 * detOrm.b - 1.0) * 0.5, 0.0, 1.0);
#endif

    float icingCoverage = getWindshieldIcingCoverage();
    if (icingCoverage > 0.0) {
        perceptualRoughness = mix(perceptualRoughness, 0.9, icingCoverage);
    }
}

vec3 applyAsoboWindshieldNormal(vec3 tangentNormal)
{
#ifdef HAS_SCRATCHES_NORMAL_MAP
    if (u_MicroScratchesStrength > 0.0) {
        vec2 uv0 = v_texcoord_0 * u_MicroScratchesTiling;
        mat2 rot60 = mat2(0.5, 0.8660254, -0.8660254, 0.5);
        mat2 rot120 = mat2(-0.5, 0.8660254, -0.8660254, -0.5);

#ifdef SCRATCHES_NORMAL_TEXTURE_RG_SNORM
        vec3 n0 = sampleTangentSpaceNormalRG(u_ScratchesNormalSampler, uv0);
        vec3 n1 = sampleTangentSpaceNormalRG(u_ScratchesNormalSampler, rot60 * uv0);
        vec3 n2 = sampleTangentSpaceNormalRG(u_ScratchesNormalSampler, rot120 * uv0);
#else
        vec3 n0 = sampleTangentSpaceNormal(u_ScratchesNormalSampler, uv0);
        vec3 n1 = sampleTangentSpaceNormal(u_ScratchesNormalSampler, rot60 * uv0);
        vec3 n2 = sampleTangentSpaceNormal(u_ScratchesNormalSampler, rot120 * uv0);
#endif
        n1.xy = rot60 * n1.xy;
        n2.xy = rot120 * n2.xy;

        vec3 nScratches = normalize(n0 + n1 + n2);
        vec3 nMicro = normalize(vec3(nScratches.xy * u_MicroScratchesStrength, 1.0));
        tangentNormal = normalize(vec3(tangentNormal.xy + nMicro.xy, tangentNormal.z * nMicro.z));
    }
#endif

#ifdef HAS_DETAIL_NORMAL_MAP
    float icingCoverage = getWindshieldIcingCoverage();
    if (icingCoverage > 0.0) {
#ifdef DETAIL_NORMAL_TEXTURE_RG_SNORM
        vec3 icingNorm = sampleTangentSpaceNormalRG(u_DetailNormalSampler, getWindshieldDetailUV());
#else
        vec3 icingNorm = sampleTangentSpaceNormal(u_DetailNormalSampler, getWindshieldDetailUV());
#endif
        icingNorm *= vec3(u_WindshieldDetailNormalScale, u_WindshieldDetailNormalScale, 1.0);
        icingNorm = normalize(icingNorm);
        tangentNormal = normalize(mix(tangentNormal, icingNorm, icingCoverage));
    }
#endif

    return tangentNormal;
}

#endif // ASOBO_MATERIAL_WINDSHIELD
