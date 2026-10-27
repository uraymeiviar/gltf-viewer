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
};
