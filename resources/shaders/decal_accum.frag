// A2.2: decal accumulation. Draws a decal's surface properties into the shared buffer in
// the A1.4 decal order. The fragment shader evaluates the decal material exactly like the
// forward PBR shader (same material_info helpers, same DirectX/packaged normal decoding),
// then writes one weighted value per target:
//   A0 base colour (rgb d*w, a w)
//   A1 world-space shading normal (rgb d*w, a w)
//   A2 emissive (rgb d*w, a w)
//   A3 metallic (r d*w, g 0, a w for the blend weight)
//   A4 roughness (r d*w, g 0, a w for the blend weight)
//   A5 occlusion factor lerp(1, O, w)
// The pass supplies the blend rules: over for A0-A4 (with the alpha channel carrying the
// accumulated T), multiply for A5. Coverage is the base-colour alpha, in one place.

layout(location = 0) out vec4 g_DecalBaseColor;
layout(location = 1) out vec4 g_DecalNormal;
layout(location = 2) out vec4 g_DecalEmissive;
layout(location = 3) out vec4 g_DecalMetallic;
layout(location = 4) out vec4 g_DecalRoughness;
layout(location = 5) out vec4 g_DecalOcclusion;

#include <camera_block.glsl>
#include <environment_block.glsl>
#include <textures.glsl>
#include <functions.glsl>
#include <material_info.glsl>

uniform float u_DecalBaseColorFactor;
uniform float u_DecalMetallicFactor;
uniform float u_DecalRoughnessFactor;
uniform float u_DecalNormalFactor;
uniform float u_DecalEmissiveFactor;
uniform float u_DecalOcclusionFactor;
uniform float u_DecalNormalOverride;
// The prepass G0: the receiver's world-space geometric normal at this pixel.
uniform sampler2D u_DecalG0;
// A2.2: the prepass depth. This pass has no depth attachment and no GL_DEPTH_TEST; the test
// below replaces it with a tolerance chosen from the measured decal-receiver gap of the E195
// cockpit (see docs/ASOBO_EXTENSIONS_PHASE_2.md, A2.2). A fixed-function LEQUAL against the
// prepass depth z-fought: the legends are coplanar with their button faces.
uniform sampler2D u_DecalDepth;

// The absolute tolerance is per viewport (metres). The relative term only covers depth
// precision; authored gaps are measured along the receiver normal, not the view ray.
const float kDecalDepthRelTolerance = 0.0002;

// Positive view-space distance of a window-space depth, for both the perspective and the
// orthographic projection (the frame's matrices). The derivation uses the z row of the
// projection matrix, so it also covers an infinite far plane:
//   ndcZ = (P22 * zEye + P32) / (P23 * zEye + P33),  windowDepth = (ndcZ + 1) / 2
float decalViewDistance(float windowDepth)
{
    float ndcZ = windowDepth * 2.0 - 1.0;
    float denominator = ndcZ * u_ProjectionMatrix[2][3] - u_ProjectionMatrix[2][2];
    if (abs(denominator) < 1e-9)
    {
        // An infinite far plane at the cleared depth: beyond any finite receiver.
        return 1e30;
    }
    float zEye = (u_ProjectionMatrix[3][2] - ndcZ * u_ProjectionMatrix[3][3]) / denominator;
    return -zEye;
}

void main()
{
    // A2.2: tolerant depth test against the prepass surface. The decal must still be hidden
    // by opaque geometry in front of it; it must not fight the surface it lies on.
    float storedDepth = texelFetch(u_DecalDepth, ivec2(gl_FragCoord.xy), 0).r;
    float decalDistance = decalViewDistance(gl_FragCoord.z);
    float surfaceDistance = decalViewDistance(storedDepth);
    vec3 receiverNormal = normalize(texelFetch(u_DecalG0, ivec2(gl_FragCoord.xy), 0).rgb * 2.0 - 1.0);
    // The third view-matrix row points towards the camera in world space. Orthographic
    // rays are parallel; perspective rays run from this fragment towards the eye.
    vec3 viewAxis = vec3(u_ViewMatrix[0][2], u_ViewMatrix[1][2], u_ViewMatrix[2][2]);
    bool orthographic = abs(u_ProjectionMatrix[2][3]) < 1e-6;
    vec3 viewDir = orthographic ? viewAxis : normalize(u_Camera - v_Position);
    // Linearized Z is axial distance, so first convert it to distance along the ray.
    float rayScale = 1.0 / max(abs(dot(viewDir, viewAxis)), 1e-6);
    float gapAlongRay = (decalDistance - surfaceDistance) * rayScale;
    float gapNormal = gapAlongRay * max(abs(dot(viewDir, receiverNormal)), 0.05);
    float tolerance = u_DecalDepthTolerance + kDecalDepthRelTolerance * decalDistance * rayScale;
    if (gapNormal > tolerance)
    {
        discard;
    }

    vec4 baseColor = getBaseColor();
    float coverage = clamp(baseColor.a, 0.0, 1.0);

#if ALPHAMODE == ALPHAMODE_MASK
    if (baseColor.a < u_AlphaCutoff)
    {
        discard;
    }
#endif

    // Each property has its own weight: w_p = factor_p * coverage (design open question 1).
    float wBase = clamp(u_DecalBaseColorFactor, 0.0, 1.0) * coverage;
    float wMetallic = clamp(u_DecalMetallicFactor, 0.0, 1.0) * coverage;
    float wRoughness = clamp(u_DecalRoughnessFactor, 0.0, 1.0) * coverage;
    float wNormal = clamp(u_DecalNormalFactor, 0.0, 1.0) * coverage;
    float wEmissive = clamp(u_DecalEmissiveFactor, 0.0, 1.0) * coverage;
    float wOcclusion = clamp(u_DecalOcclusionFactor, 0.0, 1.0) * coverage;

    if (wBase + wMetallic + wRoughness + wNormal + wEmissive + wOcclusion <= 0.0) {
        discard;
    }

    MaterialInfo materialInfo;
    materialInfo.baseColor = baseColor.rgb;
    // The same default reflectance the forward shader starts from; only metallic and
    // roughness are read back from getMetallicRoughnessInfo.
    materialInfo.ior = 1.5;
    materialInfo.f0 = vec3(0.04);
    materialInfo.specularWeight = 1.0;

    float metallic = 0.0;
    float roughness = 1.0;
#ifdef MATERIAL_METALLICROUGHNESS
    materialInfo = getMetallicRoughnessInfo(materialInfo);
    metallic = clamp(materialInfo.metallic, 0.0, 1.0);
    roughness = clamp(materialInfo.perceptualRoughness, 0.0, 1.0);
#elif defined(MATERIAL_SPECULARGLOSSINESS)
    roughness = 1.0 - clamp(u_GlossinessFactor, 0.0, 1.0);
#endif

    // Decal normal in world space. normalOverrideFactor mixes the receiver's mesh normal
    // (prepass G0) with the decal mesh normal as the base, before the decal normal map.
    vec3 v = normalize(u_Camera - v_Position);
    NormalInfo normalInfo = getNormalInfo(v);
    vec3 baseNormal = normalize(mix(receiverNormal, normalInfo.ng, clamp(u_DecalNormalOverride, 0.0, 1.0)));
    vec3 decalNormal = baseNormal;
#ifdef HAS_NORMAL_MAP
    // Rebuild the tangent frame around the overridden base normal, then apply the decal's
    // tangent-space map (sampleTangentSpaceNormal already applies DirectX/packaged rules).
    vec3 t = normalize(normalInfo.t - baseNormal * dot(baseNormal, normalInfo.t));
    vec3 b = cross(baseNormal, t);
    decalNormal = normalize(mat3(t, b, baseNormal) * normalInfo.ntex);
#endif

    vec3 emissive = u_EmissiveFactor;
    if (u_AsoboEmissiveActive != 0) {
        emissive *= u_MsfsEmissiveScale;
        if (u_AsoboMaterialEmissiveApplied != 0) {
            float dayNightMult = (u_TimeOfDay == 1) ? u_EmissiveNightMultiplier : u_EmissiveDayMultiplier;
            emissive *= dayNightMult;
        }
    } else {
        emissive = clamp(emissive, vec3(0.0), vec3(1.0));
    }
#ifdef MATERIAL_EMISSIVE_STRENGTH
    emissive *= u_EmissiveStrength;
#endif
#ifdef HAS_EMISSIVE_MAP
    emissive *= texture(u_EmissiveSampler, getEmissiveUV()).rgb;
#endif

    // Occlusion is the receiver-relative multiplier lerp(1, O_decal, w), then the pass
    // multiplies it into A5. The packaged COMP texture stores AO as sqrt(linear AO).
    float occlusion = 1.0;
#ifdef HAS_OCCLUSION_MAP
    occlusion = texture(u_OcclusionSampler, getOcclusionUV()).r;
#ifdef ASOBO_ASSET_OPTIMIZED
    occlusion *= occlusion;
#endif
#endif
    occlusion = mix(1.0, occlusion, wOcclusion);

    g_DecalBaseColor = vec4(baseColor.rgb * wBase, wBase);
    g_DecalNormal = vec4(decalNormal * wNormal, wNormal);
    g_DecalEmissive = vec4(emissive * wEmissive, wEmissive);
    // A3/A4 store V in red and T in green; the source green must be 0 so the blend yields
    // T' = T * (1 - w). The output alpha carries w for the blend factors.
    g_DecalMetallic = vec4(metallic * wMetallic, 0.0, 0.0, wMetallic);
    g_DecalRoughness = vec4(roughness * wRoughness, 0.0, 0.0, wRoughness);
    g_DecalOcclusion = vec4(occlusion, 0.0, 0.0, 1.0);
}
