// A3.2: cascaded shadow maps for the A3.1 sun (docs/ASOBO_EXTENSIONS_PHASE_3.md, A3.2).
//
// sunShadowVisibility(worldPosition, offsetNormal) is the reusable receiver lookup: 1 = the sun reaches the
// point, 0 = it is fully shadowed. It needs camera_block.glsl (u_ViewMatrix selects the cascade by view
// depth) and environment_block.glsl (matrices, splits, settings). A3.5 (AO), A3.6 (ground plane) and A3.7
// (sky portals) call the same function; a shader that includes this file must bind u_SunShadowMap (the
// CommandGenerator adds it to every command; unused samplers are skipped).
//
// offsetNormal: unit normal along which the point is pushed before the lookup (normal-offset bias, scaled
// by the cascade's texel size). The usual receiver passes its geometric normal; SSS passes the reversed
// normal to sample the back side of the surface.
//
// Filters (u_ShadowFilter): 0 hard = one bilinear hardware compare (2x2 texels); 1 PCF = 3x3 bilinear
// compares one texel apart (a smooth 4x4-texel footprint); 2 soft = 16 bilinear compares on a Poisson disk
// of radius 2.5 texels rotated per pixel (interleaved gradient noise, fixed in time, so frames repeat
// exactly and TAA only has to remove the grain). Cascade boundaries blend over the last 10% of each
// cascade's depth range into the next one.

uniform sampler2DArrayShadow u_SunShadowMap;

float sunShadowCompare(vec2 uv, float layer, float depth)
{
    // Zero gradients: the map has no mip levels, and the lookups run in non-uniform control flow.
    return textureGrad(u_SunShadowMap, vec4(uv, layer, depth), vec2(0.0), vec2(0.0));
}

float sunShadowCascade(int cascade, vec3 worldPosition, vec3 offsetNormal, float sinAngle)
{
    float texel = u_ShadowTexelSizes[cascade];
    // Normal offset: u_ShadowParams.w texels, growing with the angle between the normal and the sun (a fifth of
    // it for a surface facing the sun head-on; the caster pass's slope-scaled bias covers the filter footprint).
    // Raising the receiver moves a shadow edge on it towards the occluder by offset / tan(sun elevation).
    vec3 position = worldPosition + offsetNormal * (texel * u_ShadowParams.w * (0.2 + sinAngle));
    vec4 coord = u_ShadowMatrices[cascade] * vec4(position, 1.0);
    vec3 uvz = coord.xyz / coord.w;
    if (uvz.z >= 1.0) {
        return 1.0;
    }
    float layer = float(cascade);
    vec2 texelUv = vec2(1.0 / u_ShadowMapSize);
    float depth = clamp(uvz.z, 0.0, 1.0);
    if (u_ShadowFilter == 0) {
        return sunShadowCompare(uvz.xy, layer, depth);
    }
    if (u_ShadowFilter == 1) {
        float sum = 0.0;
        for (int y = -1; y <= 1; ++y) {
            for (int x = -1; x <= 1; ++x) {
                sum += sunShadowCompare(uvz.xy + vec2(x, y) * texelUv, layer, depth);
            }
        }
        return sum / 9.0;
    }
    const vec2 poisson[16] = vec2[16](
        vec2(-0.94201624, -0.39906216), vec2(0.94558609, -0.76890725), vec2(-0.09418410, -0.92938870),
        vec2(0.34495938, 0.29387760), vec2(-0.91588581, 0.45771432), vec2(-0.81544232, -0.87912464),
        vec2(-0.38277543, 0.27676845), vec2(0.97484398, 0.75648379), vec2(0.44323325, -0.97511554),
        vec2(0.53742981, -0.47373420), vec2(-0.26496911, -0.41893023), vec2(0.79197514, 0.19090188),
        vec2(-0.24188840, 0.99706507), vec2(-0.81409955, 0.91437590), vec2(0.19984126, 0.78641367),
        vec2(0.14383161, -0.14100790));
    // Interleaved gradient noise (Jimenez 2014) per screen pixel, constant over time.
    float angle = 6.28318530718 * fract(52.9829189 * fract(dot(floor(gl_FragCoord.xy), vec2(0.06711056, 0.00583715))));
    float c = cos(angle);
    float s = sin(angle);
    mat2 rotation = mat2(c, s, -s, c);
    float sum = 0.0;
    for (int i = 0; i < 16; ++i) {
        sum += sunShadowCompare(uvz.xy + rotation * poisson[i] * (2.5 * texelUv), layer, depth);
    }
    return sum / 16.0;
}

// The cascade that covers this view depth (u_ShadowCascadeCount when the point is beyond the last one).
int sunShadowCascadeIndex(float viewDepth)
{
    int cascade = 0;
    for (int i = 0; i < 4; ++i) {
        if (i < u_ShadowCascadeCount && viewDepth > u_ShadowSplits[i]) {
            cascade = i + 1;
        }
    }
    return cascade;
}

float sunShadowViewDepth(vec3 worldPosition)
{
    return -(u_ViewMatrix * vec4(worldPosition, 1.0)).z;
}

float sunShadowVisibility(vec3 worldPosition, vec3 offsetNormal)
{
    if (u_ShadowEnabled == 0) {
        return 1.0;
    }
    float viewDepth = sunShadowViewDepth(worldPosition);
    int cascade = sunShadowCascadeIndex(viewDepth);
    if (cascade >= u_ShadowCascadeCount) {
        return 1.0;
    }
    float NdotL = clamp(dot(offsetNormal, normalize(u_SunDirection)), -1.0, 1.0);
    float sinAngle = sqrt(max(1.0 - NdotL * NdotL, 0.0));
    float visibility = sunShadowCascade(cascade, worldPosition, offsetNormal, sinAngle);

    // Blend into the next cascade over the last 10% of this cascade's depth range.
    float start = cascade == 0 ? u_ShadowParams.x : u_ShadowSplits[cascade - 1];
    float end = u_ShadowSplits[cascade];
    float band = 0.1 * max(end - start, 1.0e-6);
    float t = (end - viewDepth) / band;
    if (t < 1.0 && cascade + 1 < u_ShadowCascadeCount) {
        float next = sunShadowCascade(cascade + 1, worldPosition, offsetNormal, sinAngle);
        visibility = mix(next, visibility, clamp(t, 0.0, 1.0));
    }
    // Fade out towards the max distance when it cuts the scene.
    if (viewDepth > u_ShadowParams.y) {
        visibility = mix(visibility, 1.0, clamp((viewDepth - u_ShadowParams.y) / max(u_ShadowParams.z - u_ShadowParams.y, 1.0e-6), 0.0, 1.0));
    }
    return visibility;
}

// Debug view colour of the cascade at this point (red, green, blue, yellow; grey beyond the last cascade or
// when shadows are off), darkened where the point is shadowed.
vec3 sunShadowCascadeDebugColor(vec3 worldPosition, vec3 offsetNormal)
{
    if (u_ShadowEnabled == 0) {
        return vec3(0.25);
    }
    int cascade = sunShadowCascadeIndex(sunShadowViewDepth(worldPosition));
    if (cascade >= u_ShadowCascadeCount) {
        return vec3(0.25);
    }
    const vec3 colors[4] = vec3[4](vec3(1.0, 0.25, 0.25), vec3(0.25, 1.0, 0.25), vec3(0.3, 0.45, 1.0), vec3(1.0, 1.0, 0.25));
    return colors[cascade] * mix(0.3, 1.0, sunShadowVisibility(worldPosition, offsetNormal));
}
