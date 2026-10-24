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
};
