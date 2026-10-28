// A3.5: screen-space ambient occlusion for the sky light (docs/ASOBO_EXTENSIONS_PHASE_3.md, A3.5).
// Ground-truth ambient occlusion (GTAO, Jimenez et al. 2016, "Practical Real-Time Strategies for Accurate
// Indirect Occlusion"; the slice/horizon formulation follows Intel's XeGTAO) from the A2.1 prepass depth and
// world-space geometric normal. Three passes share this file (header/imgui/AmbientOcclusion.h prepends
// #version and one PASS_* define):
//   PASS_GTAO     visibility at the AO resolution (full or half), fixed 4 x 4 interleaved noise;
//   PASS_DENOISE  depth-aware 5 x 5 filter whose [0.5 1 1 1 0.5] taps give every noise phase the same weight;
//   PASS_UPSAMPLE half resolution only: depth-aware bilinear upsample to the full-resolution target.
// Every pass writes RG32F: x = visibility (1 = unoccluded), y = linear view depth of the prepass surface
// (-1 on the background). pbr.frag (ssao.glsl) reads x where its own depth matches y.
// Nothing changes over time: TAA integrates the sub-pixel jitter only, so the term cannot shimmer.

out vec4 g_Output;

uniform ivec2 u_FullSize;   // render size in full-resolution pixels
uniform ivec2 u_AoSize;     // AO target size (= u_FullSize, or half of it rounded up)
uniform int u_AoScale;      // 1 full, 2 half resolution

const float PI = 3.14159265358979;
const float PI_HALF = 1.57079632679490;

#ifdef PASS_GTAO
uniform sampler2D u_Depth;   // prepass depth (window depth 0..1, the TAA-jittered projection)
uniform sampler2D u_Normal;  // prepass world-space geometric normal, n * 0.5 + 0.5
uniform mat4 u_InvProjection; // inverse of the projection the prepass was drawn with
uniform mat3 u_ViewRotation;  // world -> view rotation, for the normals
uniform float u_ProjScaleY;   // projection[1][1]
uniform int u_Orthographic;
uniform float u_Radius;       // world (view) units
uniform float u_Intensity;    // exponent on the visibility
uniform int u_SliceCount;
uniform int u_StepCount;

vec3 viewPositionAt(ivec2 pixel, float depth)
{
    vec2 ndc = (vec2(pixel) + 0.5) / vec2(u_FullSize) * 2.0 - 1.0;
    vec4 view = u_InvProjection * vec4(ndc, depth * 2.0 - 1.0, 1.0);
    return view.xyz / view.w;
}

// 4 x 4 ordered (Bayer) pattern in [0, 1): periodic over four pixels in x and y, so the denoise filter cancels it.
float bayer4(ivec2 p)
{
    const float m[16] = float[16](0.0, 8.0, 2.0, 10.0, 12.0, 4.0, 14.0, 6.0, 3.0, 11.0, 1.0, 9.0, 15.0, 7.0, 13.0, 5.0);
    return (m[(p.y & 3) * 4 + (p.x & 3)] + 0.5) / 16.0;
}

void main()
{
    ivec2 aoPixel = ivec2(gl_FragCoord.xy);
    ivec2 pixel = min(aoPixel * u_AoScale, u_FullSize - 1);
    float depth = texelFetch(u_Depth, pixel, 0).r;
    if (depth >= 1.0) {
        g_Output = vec4(1.0, -1.0, 0.0, 1.0);
        return;
    }
    vec3 position = viewPositionAt(pixel, depth);
    vec3 normal = normalize(u_ViewRotation * (texelFetch(u_Normal, pixel, 0).xyz * 2.0 - 1.0));
    vec3 viewVec = u_Orthographic != 0 ? vec3(0.0, 0.0, 1.0) : normalize(-position);
    float viewDepth = -position.z;

    // The world radius projected to full-resolution pixels at this depth.
    float screenRadius = 0.5 * float(u_FullSize.y) * u_ProjScaleY * u_Radius / (u_Orthographic != 0 ? 1.0 : max(viewDepth, 1e-6));
    if (screenRadius < 1.0) {
        g_Output = vec4(1.0, viewDepth, 0.0, 1.0);
        return;
    }
    // Samples closer than ~1.3 pixels would read this pixel itself.
    float minS = 1.3 / screenRadius;
    // Distance falloff: full weight up to 60% of the radius, none beyond it (open space stays unoccluded).
    float falloffRange = 0.4 * u_Radius;
    float falloffMul = -1.0 / falloffRange;
    float falloffAdd = u_Radius / falloffRange;

    float noiseSlice = bayer4(aoPixel);
    float noiseStep = bayer4(aoPixel + ivec2(2, 1));
    float visibility = 0.0;
    // The same slices with every horizon at the tangent plane: dividing by it makes the slice quadrature exact for an
    // open surface (a few slices otherwise leave a tilted, unoccluded surface a few percent off 1).
    float unoccluded = 0.0;
    for (int slice = 0; slice < u_SliceCount; ++slice) {
        float phi = (float(slice) + noiseSlice) * PI / float(u_SliceCount);
        vec2 omega = vec2(cos(phi), sin(phi));
        // OpenGL: window y and view y both point up, so the slice direction is the same in both.
        vec3 directionVec = vec3(omega, 0.0);
        vec3 orthoDirectionVec = directionVec - dot(directionVec, viewVec) * viewVec;
        vec3 axisVec = normalize(cross(orthoDirectionVec, viewVec));
        vec3 projectedNormal = normal - axisVec * dot(normal, axisVec);
        float projectedNormalLength = length(projectedNormal);
        if (projectedNormalLength < 1e-4) {
            continue;
        }
        float signNorm = sign(dot(orthoDirectionVec, projectedNormal));
        float cosNorm = clamp(dot(projectedNormal, viewVec) / projectedNormalLength, 0.0, 1.0);
        float n = signNorm * acos(cosNorm);
        // Horizons as elevations above the surface's tangent plane, per side (0: +omega, 1: -omega). A sample's
        // in-slice elevation is asin(dot(d, normal) / |projected normal|) for a sample in the slice plane; using the
        // 3D normal keeps a sample that pixel rounding moved off the slice on the same plane at elevation 0, so a
        // plane stays unoccluded at grazing angles (the cosine of the in-slice angle would lift its horizon).
        // Samples beyond the radius fade to elevation 0 (the falloff of the paper's horizon cosine).
        float elevation0 = 0.0;
        float elevation1 = 0.0;

        for (int stepIndex = 0; stepIndex < u_StepCount; ++stepIndex) {
            float stepNoise = fract(noiseStep + float(slice + stepIndex * u_StepCount) * 0.6180339887);
            float s = (float(stepIndex) + stepNoise) / float(u_StepCount);
            s = s * s + minS;
            vec2 offset = round(s * screenRadius * omega);
            ivec2 p0 = pixel + ivec2(offset);
            ivec2 p1 = pixel - ivec2(offset);
            if (all(greaterThanEqual(p0, ivec2(0))) && all(lessThan(p0, u_FullSize))) {
                float d0 = texelFetch(u_Depth, p0, 0).r;
                if (d0 < 1.0) {
                    vec3 delta0 = viewPositionAt(p0, d0) - position;
                    float dist0 = length(delta0);
                    if (dist0 > 1e-6) {
                        float e0 = clamp(dot(delta0, normal) / (dist0 * projectedNormalLength), 0.0, 1.0);
                        elevation0 = max(elevation0, e0 * clamp(dist0 * falloffMul + falloffAdd, 0.0, 1.0));
                    }
                }
            }
            if (all(greaterThanEqual(p1, ivec2(0))) && all(lessThan(p1, u_FullSize))) {
                float d1 = texelFetch(u_Depth, p1, 0).r;
                if (d1 < 1.0) {
                    vec3 delta1 = viewPositionAt(p1, d1) - position;
                    float dist1 = length(delta1);
                    if (dist1 > 1e-6) {
                        float e1 = clamp(dot(delta1, normal) / (dist1 * projectedNormalLength), 0.0, 1.0);
                        elevation1 = max(elevation1, e1 * clamp(dist1 * falloffMul + falloffAdd, 0.0, 1.0));
                    }
                }
            }
        }

        // h0 / h1: the horizon angles from the view vector on the -omega / +omega side (Jimenez's h1 / h2).
        float h0 = n - PI_HALF + asin(elevation1);
        float h1 = n + PI_HALF - asin(elevation0);
        float iarc0 = (cosNorm + 2.0 * h0 * sin(n) - cos(2.0 * h0 - n)) / 4.0;
        float iarc1 = (cosNorm + 2.0 * h1 * sin(n) - cos(2.0 * h1 - n)) / 4.0;
        visibility += projectedNormalLength * (iarc0 + iarc1);
        // Open: h = n -/+ pi/2, iarc0 + iarc1 = cos(n) + n sin(n).
        unoccluded += projectedNormalLength * (cosNorm + n * sin(n));
    }
    visibility = unoccluded > 1e-4 ? clamp(visibility / unoccluded, 0.0, 1.0) : 1.0;
    visibility = pow(visibility, u_Intensity);
    g_Output = vec4(visibility, viewDepth, 0.0, 1.0);
}
#endif

#ifdef PASS_DENOISE
uniform sampler2D u_Ao;       // RG32F at the AO resolution
uniform float u_DepthTolerance; // relative view-depth difference that halves a tap's weight

void main()
{
    ivec2 pixel = ivec2(gl_FragCoord.xy);
    vec2 center = texelFetch(u_Ao, pixel, 0).xy;
    if (center.y < 0.0) {
        g_Output = vec4(1.0, -1.0, 0.0, 1.0);
        return;
    }
    // Neighbours on the same surface have a depth that changes linearly across the pixel: compare each tap with
    // the depth extrapolated from the centre's screen-space depth gradient (central differences, clamped).
    float dl = texelFetch(u_Ao, clamp(pixel - ivec2(1, 0), ivec2(0), u_AoSize - 1), 0).y;
    float dr = texelFetch(u_Ao, clamp(pixel + ivec2(1, 0), ivec2(0), u_AoSize - 1), 0).y;
    float dd = texelFetch(u_Ao, clamp(pixel - ivec2(0, 1), ivec2(0), u_AoSize - 1), 0).y;
    float du = texelFetch(u_Ao, clamp(pixel + ivec2(0, 1), ivec2(0), u_AoSize - 1), 0).y;
    float gx = (dl > 0.0 && dr > 0.0) ? (abs(dr - center.y) < abs(center.y - dl) ? dr - center.y : center.y - dl) : 0.0;
    float gy = (dd > 0.0 && du > 0.0) ? (abs(du - center.y) < abs(center.y - dd) ? du - center.y : center.y - dd) : 0.0;
    float tolerance = u_DepthTolerance * center.y;
    float sum = 0.0;
    float weightSum = 0.0;
    for (int y = -2; y <= 2; ++y) {
        for (int x = -2; x <= 2; ++x) {
            ivec2 p = pixel + ivec2(x, y);
            if (any(lessThan(p, ivec2(0))) || any(greaterThanEqual(p, u_AoSize))) {
                continue;
            }
            vec2 tap = texelFetch(u_Ao, p, 0).xy;
            if (tap.y < 0.0) {
                continue;
            }
            float expected = center.y + gx * float(x) + gy * float(y);
            float r = abs(tap.y - expected) / tolerance;
            float w = (abs(x) == 2 ? 0.5 : 1.0) * (abs(y) == 2 ? 0.5 : 1.0) / (1.0 + r * r);
            sum += tap.x * w;
            weightSum += w;
        }
    }
    g_Output = vec4(weightSum > 0.0 ? sum / weightSum : center.x, center.y, 0.0, 1.0);
}
#endif

#ifdef PASS_UPSAMPLE
uniform sampler2D u_Ao;        // RG32F at half resolution (denoised)
uniform sampler2D u_Depth;     // full-resolution prepass depth
uniform mat4 u_InvProjection;
uniform float u_DepthTolerance;

void main()
{
    ivec2 pixel = ivec2(gl_FragCoord.xy);
    float depth = texelFetch(u_Depth, pixel, 0).r;
    if (depth >= 1.0) {
        g_Output = vec4(1.0, -1.0, 0.0, 1.0);
        return;
    }
    vec2 ndc = (vec2(pixel) + 0.5) / vec2(u_FullSize) * 2.0 - 1.0;
    vec4 view = u_InvProjection * vec4(ndc, depth * 2.0 - 1.0, 1.0);
    float viewDepth = -view.z / view.w;

    // Half-resolution sample q sits on full-resolution pixel 2q: the four around this pixel, bilinear weights
    // times a depth similarity weight; with no similar sample the closest depth wins (no bleeding across edges).
    ivec2 base = pixel / 2;
    vec2 f = vec2(pixel - base * 2) * 0.5;
    float tolerance = u_DepthTolerance * viewDepth;
    float sum = 0.0;
    float weightSum = 0.0;
    float bestDifference = 1e30;
    float bestValue = 1.0;
    for (int y = 0; y <= 1; ++y) {
        for (int x = 0; x <= 1; ++x) {
            ivec2 q = min(base + ivec2(x, y), u_AoSize - 1);
            vec2 tap = texelFetch(u_Ao, q, 0).xy;
            if (tap.y < 0.0) {
                continue;
            }
            float difference = abs(tap.y - viewDepth);
            if (difference < bestDifference) {
                bestDifference = difference;
                bestValue = tap.x;
            }
            float bilinear = (x == 0 ? 1.0 - f.x : f.x) * (y == 0 ? 1.0 - f.y : f.y);
            float r = difference / tolerance;
            float w = bilinear / (1.0 + r * r * r * r);
            sum += tap.x * w;
            weightSum += w;
        }
    }
    float value = weightSum > 1e-3 ? sum / weightSum : bestValue;
    g_Output = vec4(value, viewDepth, 0.0, 1.0);
}
#endif
