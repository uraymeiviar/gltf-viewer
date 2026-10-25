// A2.2: the forward receiver reads the shared decal accumulation targets at the pixel
// centre. The buffer is always single-sample at render scale, while the forward target
// may be MSAA; gl_FragCoord is the pixel centre in both cases and matches the buffer's
// texel grid (the shared buffer is bound with the same viewport origin and size).
//
// A0 base colour (rgb V, a T), A1 world normal (rgb V, a T), A2 emissive (rgb V, a T),
// A3 metallic (r V, g T), A4 roughness (r V, g T), A5 occlusion product. p = V + T * p_below,
// so a T of exactly 1 means "no decal" and an explicit branch keeps the receiver value
// byte-identical. The targets are RGBA8/RGBA16F/RG8/R8 with nearest filtering.

#ifdef ASOBO_DECAL_RECEIVER
uniform sampler2D u_DecalA0;
uniform sampler2D u_DecalA1;
uniform sampler2D u_DecalA2;
uniform sampler2D u_DecalA3;
uniform sampler2D u_DecalA4;
uniform sampler2D u_DecalA5;

struct DecalReceiverInfo
{
    vec4 baseColor;
    vec4 normal;
    vec4 emissive;
    vec2 metallic;
    vec2 roughness;
    float occlusion;
};

DecalReceiverInfo getDecalReceiverInfo()
{
    DecalReceiverInfo info;
    info.baseColor = vec4(0.0, 0.0, 0.0, 1.0);
    info.normal = vec4(0.0, 0.0, 0.0, 1.0);
    info.emissive = vec4(0.0, 0.0, 0.0, 1.0);
    info.metallic = vec2(0.0, 1.0);
    info.roughness = vec2(0.0, 1.0);
    info.occlusion = 1.0;

    if (u_AsoboDecalReceiver != 0) {
        ivec2 pixel = ivec2(gl_FragCoord.xy);
        info.baseColor = texelFetch(u_DecalA0, pixel, 0);
        info.normal = texelFetch(u_DecalA1, pixel, 0);
        info.emissive = texelFetch(u_DecalA2, pixel, 0);
        info.metallic = texelFetch(u_DecalA3, pixel, 0).rg;
        info.roughness = texelFetch(u_DecalA4, pixel, 0).rg;
        info.occlusion = texelFetch(u_DecalA5, pixel, 0).r;
    }
    return info;
}

vec3 applyDecalColor(vec3 receiver, vec4 decal)
{
    return decal.a < 1.0 ? decal.rgb + decal.a * receiver : receiver;
}

float applyDecalScalar(float receiver, vec2 decal)
{
    return decal.y < 1.0 ? decal.x + decal.y * receiver : receiver;
}

float applyDecalOcclusion(float receiver, float decal)
{
    return decal < 1.0 ? receiver * decal : receiver;
}
#endif
