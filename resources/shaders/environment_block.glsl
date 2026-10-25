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
};
