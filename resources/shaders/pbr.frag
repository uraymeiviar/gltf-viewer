//
// This fragment shader defines a reference implementation for Physically Based Shading of
// a microfacet surface material defined by a glTF model.
//
// References:
// [1] Real Shading in Unreal Engine 4
//     http://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf
// [2] Physically Based Shading at Disney
//     http://blog.selfshadow.com/publications/s2012-shading-course/burley/s2012_pbs_disney_brdf_notes_v3.pdf
// [3] README.md - Environment Maps
//     https://github.com/KhronosGroup/glTF-WebGL-PBR/#environment-maps
// [4] "An Inexpensive BRDF Model for Physically based Rendering" by Christophe Schlick
//     https://www.cs.virginia.edu/~jdl/bib/appearance/analytic%20models/schlick94b.pdf
// [5] "KHR_materials_clearcoat"
//     https://github.com/KhronosGroup/glTF/tree/master/extensions/2.0/Khronos/KHR_materials_clearcoat

precision highp float;


#include <camera_block.glsl>
#include <environment_block.glsl>
#include <tonemapping.glsl>
#include <textures.glsl>
#include <functions.glsl>
#include <brdf.glsl>
#include <punctual.glsl>
#include <ibl.glsl>
#include <decal_receiver.glsl>
#include <asobo_detail_map.glsl>
#include <asobo_windshield.glsl>
#include <material_info.glsl>

#ifdef MATERIAL_IRIDESCENCE
#include <iridescence.glsl>
#endif


out vec4 g_finalColor;

#ifdef ASOBO_GLASS_LEGACY
uniform float u_AsoboGlassReflectionMask;
#endif

#if defined(ASOBO_GLASS_REFRACTION) || defined(ASOBO_WINDSHIELD_SSR)
uniform int u_AsoboGlassCopyValid;
#endif
#ifdef ASOBO_GLASS_REFRACTION
uniform float u_AsoboGlassWidth;

vec3 getAsoboGlassBehind(vec3 n, vec3 v, float ior, float perceptualRoughness)
{
    // Parallel slab, width in scene units. Dividing by the normal component gives
    // the travel distance to the exit plane (not simply the length of the ray).
    vec3 ray = refract(-v, n, 1.0 / ior);
    vec3 exitPosition = v_Position + ray * (u_AsoboGlassWidth / max(0.001, abs(dot(ray, n))));
    vec4 clip = u_ViewProjectionMatrix * vec4(exitPosition, 1.0);
    vec4 surfaceClip = u_ViewProjectionMatrix * vec4(v_Position, 1.0);
    // Anchor to the raster pixel, then add only the thickness-dependent displacement.
    // The full-resolution copy shares this viewport's origin, dimensions and Y direction.
    vec2 size = vec2(textureSize(u_TransmissionFramebufferSampler, 0));
    vec2 uv = gl_FragCoord.xy / size;
    uv += 0.5 * (clip.xy / clip.w - surfaceClip.xy / surfaceClip.w);
    float maxLod = floor(log2(max(1.0, max(size.x, size.y))));
    float roughBlur = max(0.0, perceptualRoughness - 0.08) / 0.92;
    float lod = clamp(maxLod * applyIorToRoughness(roughBlur, ior), 0.0, maxLod);
    vec2 border = 0.5 * exp2(lod) / size;
    return textureLod(u_TransmissionFramebufferSampler, clamp(uv, border, 1.0 - border), lod).rgb;
}
#endif


#ifdef ASOBO_WINDSHIELD_SSR
uniform sampler2D u_SsrDepth;
uniform int u_SsrDepthFloat; // 0: resolved D24; 1: decal prepass D32F
uniform float u_SsrFrame; // ray phase noise frame (TAA accumulation), 0 without TAA
uniform vec3 u_SsrOptions; // enabled, steps, interior sky strength
uniform float u_SsrAttenuation;
uniform int u_HasInteriorVolume;
uniform vec3 u_InteriorMin;
uniform vec3 u_InteriorMax;

float ssrViewZ(float depth)
{
    // Solve clip.z / clip.w for view Z; works for perspective and orthographic
    // projections, including the XY jitter used by TAA, without a matrix inverse.
    float z = depth * 2.0 - 1.0;
    return (u_ProjectionMatrix[3][2] - z * u_ProjectionMatrix[3][3]) /
           (z * u_ProjectionMatrix[2][3] - u_ProjectionMatrix[2][2]);
}

// Propagate one stored depth ULP through the actual projection. D32F spacing
// depends on the exponent; copied D24 is uniformly quantized in window depth.
float ssrDepthPrecision(float depth)
{
    float ulp = u_SsrDepthFloat != 0
        ? max(uintBitsToFloat(floatBitsToUint(depth) + 1u) - depth, 1.0e-12)
        : 1.0 / 16777215.0;
    return max(abs(ssrViewZ(min(depth + ulp, 0.99999994)) -
                   ssrViewZ(max(depth - ulp, 0.0))) * 0.5, 1.0e-7);
}

float ssrResidualTolerance(float depth)
{
    // Reconstruction/plane fitting combines several rounded depth samples.
    // The small floor covers view-matrix float arithmetic, not scene thickness.
    return max(0.0001, 16.0 * ssrDepthPrecision(depth));
}

vec3 ssrPosition(vec2 uv, float depth)
{
    vec4 p = inverse(u_ProjectionMatrix) * vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    return p.xyz / p.w;
}

// View-space depth of the opaque surface at uv, bilinear between the four texels when they
// lie on one continuous surface; nearest across silhouettes and sky (no false bridging).
float ssrContinuousZ(vec2 uv, vec2 size)
{
    vec2 coord = uv * size - 0.5;
    ivec2 base = ivec2(floor(coord));
    vec2 f = coord - vec2(base);
    float z[4];
    float lowest = 1.0e9, highest = -1.0e9;
    bool sky = false;
    for (int corner = 0; corner < 4; ++corner) {
        ivec2 q = clamp(base + ivec2(corner & 1, corner >> 1), ivec2(0), ivec2(size) - 1);
        float d = texelFetch(u_SsrDepth, q, 0).r;
        sky = sky || d >= 0.999999;
        z[corner] = d >= 0.999999 ? -1.0e9 : ssrViewZ(d);
        lowest = min(lowest, z[corner]);
        highest = max(highest, z[corner]);
    }
    if (sky || highest - lowest > max(0.02, 0.02 * abs(highest))) {
        int nearest = (f.x >= 0.5 ? 1 : 0) + (f.y >= 0.5 ? 2 : 0);
        return z[nearest];
    }
    return mix(mix(z[0], z[1], f.x), mix(z[2], z[3], f.x), f.y);
}

// Percentage-closer coverage of a hit (as PCF for shadow maps): the share of a smooth
// 4x4 tent footprint whose depth texels lie on the hit surface. At grazing angles many
// windshield pixels reflect one source texel, and the yes/no hit decision magnified the
// source's pixel staircase along reflected silhouettes into large blocks. The coverage
// varies within a texel, so the reflection fades out over its last texel instead.
// The tent covers +-radius source texels on a grid of radius/1.5 texels: the weights still
// vary continuously with uv, so a wider (blurred) footprint never brings the steps back.
float ssrHitCoverage(vec2 uv, float rayZ, vec2 size, float radius)
{
    float spacing = max(radius / 1.5, 1.0);
    vec2 grid = (uv * size - 0.5) / spacing;
    vec2 base = floor(grid) - 1.0;
    vec2 f = grid - floor(grid);
    // Same-surface tolerance: generous enough for a grazing continuous surface across
    // the footprint, far below a silhouette's depth jump.
    float tolerance = max(0.03, 0.04 * abs(rayZ));
    float covered = 0.0, total = 0.0;
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            vec2 offset = vec2(x - 1, y - 1) - f;
            vec2 tent = max(vec2(0.0), 1.5 - abs(offset));
            float weight = tent.x * tent.y;
            ivec2 q = clamp(ivec2(round((base + vec2(x, y)) * spacing)), ivec2(0), ivec2(size) - 1);
            float d = texelFetch(u_SsrDepth, q, 0).r;
            float onSurface = d >= 0.999999 ? 0.0 :
                1.0 - smoothstep(0.5, 1.0, abs(ssrViewZ(d) - rayZ) / tolerance);
            covered += weight * onSurface;
            total += weight;
        }
    }
    // The nearest-texel decision flips where coverage is one half: fade to zero there.
    return smoothstep(0.5, 0.9, covered / total);
}

// Returns hit UV and confidence, not colour. Opaque depth/colour exclude transparents.
// An obstructed/discontinuous ray terminates: never search through it for another hit.
vec4 traceWindshield(vec3 n, vec3 v, vec3 ng, vec3 worldPosition)
{
    if (u_SsrOptions.x == 0.0 || u_SsrAttenuation <= 0.0 || u_AsoboGlassCopyValid == 0) return vec4(0);
    ng = dot(ng, v) >= 0.0 ? ng : -ng;
    // Trace about the smooth geometric normal. The windshield's tiled detail/scratch normal
    // map bent each pixel's ray differently, giving stippled, moire-rippled reflections;
    // the shaded normal still drives the Fresnel/specular weight of the result.
    vec3 reflected = reflect(-v, ng);
    if (dot(reflected, ng) <= 0.02) return vec4(0); // reject rays below the physical pane
    vec3 ray = normalize(mat3(u_ViewMatrix) * reflected);
    vec3 origin = (u_ViewMatrix * vec4(worldPosition + ng * 0.001, 1.0)).xyz + ray * 0.002;
    float maxTravel = 2.0; // cockpit-scale trace, not a five-metre minimum
    if (ray.z > 0.0) maxTravel = min(maxTravel, (-0.01 - origin.z) / ray.z);
    if (maxTravel <= 0.003) return vec4(0);
    vec3 end = origin + ray * maxTravel;
    vec4 a = u_ProjectionMatrix * vec4(origin, 1.0);
    vec4 b = u_ProjectionMatrix * vec4(end, 1.0);
    vec2 size = vec2(textureSize(u_TransmissionFramebufferSampler, 0));
    vec2 startUV = a.xy / a.w * 0.5 + 0.5;
    vec2 endUV = b.xy / b.w * 0.5 + 0.5;
    vec2 span = (endUV - startUV) * size;
    float pixels = max(abs(span.x), abs(span.y));
    if (pixels < 1.0) return vec4(0);
    float stride = u_SsrOptions.y < 512.0 ? 2.0 : 1.0;
    float increment = stride / pixels;
    // Interleaved gradient noise (Jimenez 2014) on the ray's start phase. Neighbouring
    // windshield rays are nearly parallel and visit nearly the same pixels, so near a
    // reflected silhouette the hit/occluded decision flipped in long coherent teeth. The
    // noise breaks them into fine grain that TAA resolves; hits on continuous surfaces
    // are refined independently of the phase.
    vec2 noisePixel = floor(startUV * size) + 5.588238 * u_SsrFrame;
    float phase = fract(52.9829189 * fract(dot(noisePixel, vec2(0.06711056, 0.00583715))));
    vec3 qa = origin / a.w, qb = end / b.w;
    float ka = 1.0 / a.w, kb = 1.0 / b.w;
    float previousS = 0.0;
    float previousDepth = texelFetch(u_SsrDepth, ivec2(clamp(startUV * size, vec2(0), size - 1.0)), 0).r;
    float previousZ = previousDepth >= 0.999999 ? -1.0e9 : ssrViewZ(previousDepth);
    float previousDelta = previousZ - origin.z;
    for (int i = 1; i <= 1024; ++i) {
        if (i > int(u_SsrOptions.y) || float(i)*stride > 0.65*min(size.x,size.y)) break;
        float t = min((float(i) - phase) * increment, 1.0);
        vec2 uv = mix(startUV, endUV, t);
        if (any(lessThanEqual(uv, 1.0 / size)) || any(greaterThanEqual(uv, 1.0 - 1.0 / size))) break;
        vec3 point = mix(qa, qb, t) / mix(ka, kb, t);
        float depth = texelFetch(u_SsrDepth, ivec2(uv * size), 0).r;
        // Sky is infinitely far: the ray is certainly in front of it. It is never a hit,
        // and it is a valid "in front" sample before one (rays reach the glareshield by
        // passing over the sky seen through the windshield).
        float surfaceZ = depth >= 0.999999 ? -1.0e9 : ssrViewZ(depth);
        // Test the ray's depth interval across this pixel's step, not the sample point
        // (McGuire & Mara 2014). The ray's depth sweeps a range within one pixel; a point
        // sample caught a thin reflected surface only for some step phases, so neighbouring
        // columns alternated hit/miss (comb stripes).
        vec3 nearEnd = mix(qa,qb,max(t - 0.5*increment, 0.0)) / mix(ka,kb,max(t - 0.5*increment, 0.0));
        vec3 farEnd = mix(qa,qb,min(t + 0.5*increment, 1.0)) / mix(ka,kb,min(t + 0.5*increment, 1.0));
        float rayNear = max(nearEnd.z, farEnd.z), rayFar = min(nearEnd.z, farEnd.z);
        float delta = surfaceZ - point.z;
        // Assumed thickness of a depth-buffer surface. A ray further behind it passed
        // under a foreground object (e.g. the glareshield lip) and is occluded: stop,
        // never search on.
        float thickness = max(0.01, 2.0 * ssrResidualTolerance(depth));
        if (surfaceZ - rayFar >= 0.0) {
            if (previousDelta >= 0.0) return vec4(0);
            // Refine against the continuous (bilinear) depth surface, not per-pixel planes.
            // Which texel first detects the crossing jumps between neighbouring windshield
            // pixels; a hit built from that texel's plane jumped with it and drew concentric
            // contour bands. The bracket spans this pixel's step and the previous one.
            float lo = max(t - 1.5*increment, 0.0), hi = t;
            float loDelta = ssrContinuousZ(mix(startUV,endUV,lo), size) -
                            (mix(qa,qb,lo) / mix(ka,kb,lo)).z;
            float hiDelta = ssrContinuousZ(mix(startUV,endUV,hi), size) - point.z;
            if (hiDelta < 0.0) {
                hi = min(t + 0.5*increment, 1.0);
                hiDelta = ssrContinuousZ(mix(startUV,endUV,hi), size) - farEnd.z;
            }
            // Without a clean front/behind bracket (a surface thinner than the step), the
            // sample itself is the hit, judged by the interval's depth behind the surface.
            float behind = surfaceZ - rayNear;
            bool bracketed = loDelta < 0.0 && hiDelta >= 0.0;
            if (!bracketed) {
                if (behind > thickness) return vec4(0);
                lo = hi = t; loDelta = -1.0; hiDelta = 0.0;
            }
            for (int j = 0; j < 8; ++j) {
                float mid = (lo + hi) * 0.5;
                float midDelta = ssrContinuousZ(mix(startUV,endUV,mid), size) -
                                 (mix(qa,qb,mid) / mix(ka,kb,mid)).z;
                if (midDelta < 0.0) { lo = mid; loDelta = midDelta; }
                else { hi = mid; hiDelta = midDelta; }
            }
            // On a continuous surface the refined bracket closes onto the crossing; across
            // a silhouette it keeps the full depth jump: the ray went under an occluder.
            // This is independent of the surface's slope, so grazing surfaces never stripe.
            if (bracketed) behind = hiDelta - loDelta;
            if (behind > thickness) return vec4(0);
            // Linear zero crossing inside the final bracket keeps the hit continuous.
            float s = loDelta < 0.0 && hiDelta >= 0.0 ? mix(lo, hi, -loDelta / max(hiDelta - loDelta, 1.0e-9)) : hi;
            vec2 hitUV = mix(startUV, endUV, s);
            point = mix(qa,qb,s) / mix(ka,kb,s);
            float travel = length(point - origin);
            if (travel <= 0.003 || travel >= maxTravel) return vec4(0);
            if (texelFetch(u_SsrDepth, ivec2(clamp(hitUV*size, vec2(0), size-1.0)), 0).r >= 0.999999) return vec4(0);

            float edge = min(min(hitUV.x,hitUV.y), min(1.0-hitUV.x,1.0-hitUV.y));
            float screenTravel = length((hitUV-startUV)*size) / min(size.x,size.y);
            float confidence = smoothstep(0.0,0.08,edge) * (1.0-travel/maxTravel);
            confidence *= 1.0-smoothstep(0.15,0.65,screenTravel);
            // Fade, not cut, where the ray approaches the obstruction limit: the hard
            // reject drew stair-stepped edges along reflected silhouettes.
            confidence *= 1.0-smoothstep(0.7,1.0,behind/thickness);
            return vec4(hitUV, point.z, confidence); // coverage is applied by windshieldSample
        }
        previousS = t;
        previousDepth = depth;
        previousDelta = delta;
        previousZ = surfaceZ;
        if (t >= 1.0) break;
    }
    return vec4(0);
}

vec4 windshieldSample(vec3 n, vec3 v, vec3 ng, vec3 position, float roughness)
{
    vec4 hit = traceWindshield(n, v, ng, position);
    vec2 size = vec2(textureSize(u_TransmissionFramebufferSampler, 0));
    // Source texels per windshield pixel, from the quad's hit UVs (taken before any
    // per-pixel branch). A neighbour without a hit gives a meaningless jump: ignore it.
    float footprint = length(fwidth(hit.xy * size));
    if (hit.a <= 0.0) return vec4(0);
    float magnification = footprint > 0.0 && footprint < 8.0 ? 1.0 / footprint : 1.0;
    // Blur radius in source texels. Magnified: at least about half a windshield-pixel
    // staircase step, so its edges blend over several pixels. Rough glass: the GGX
    // lobe's cone (alpha = roughness^2) widened over the travelled distance.
    vec4 depthClip = u_ProjectionMatrix * vec4(0.0, 0.0, hit.z, 1.0);
    vec3 hitView = ssrPosition(hit.xy, depthClip.z / depthClip.w * 0.5 + 0.5);
    float travel = length(hitView - (u_ViewMatrix * vec4(position, 1.0)).xyz);
    float focal = 0.5 * size.y * u_ProjectionMatrix[1][1];
    float cone = travel * roughness * roughness * focal / max(-hit.z, 1.0e-3);
    float radius = clamp(max(0.5 * magnification, cone), 1.5, 16.0);
    float coverage = ssrHitCoverage(hit.xy, hit.z, size, radius);
    // The copy's mip chain is regenerated after every capture.
    vec3 colour = textureLod(u_TransmissionFramebufferSampler, hit.xy, log2(radius / 1.5)).rgb;
    float confidence = hit.a * coverage;
    return vec4(colour * confidence, confidence);
}

vec4 smoothWindshield(vec3 n, vec3 v, vec3 ng, float roughness)
{
    vec3 px = dFdx(v_Position), py = dFdy(v_Position);
    vec3 nx = dFdx(n), ny = dFdy(n);
    vec4 center = windshieldSample(n,v,ng,v_Position,roughness);
    vec4 taps[4];
    float edge[4];
    const vec2 offsets[4] = vec2[4](vec2(-1,0),vec2(1,0),vec2(0,-1),vec2(0,1));
    for (int i = 0; i < 4; ++i) {
        vec3 position = v_Position + px*offsets[i].x + py*offsets[i].y;
        vec3 normal = normalize(n + nx*offsets[i].x + ny*offsets[i].y);
        // No random rotations or frame-dependent offsets. A normal discontinuity
        // cannot contribute across the filter, and every tap retains all hit guards.
        edge[i] = smoothstep(0.7,0.98,dot(n,normal));
        taps[i] = windshieldSample(normal,normalize(u_Camera-position),ng,position,roughness);
    }
    vec4 horizontal = (taps[0]*edge[0] + 2.0*center + taps[1]*edge[1]) / (2.0+edge[0]+edge[1]);
    vec4 vertical = (taps[2]*edge[2] + 2.0*center + taps[3]*edge[3]) / (2.0+edge[2]+edge[3]);
    // Select the direction with more pixel-scale curvature. This removes both
    // row/column combs with five traces instead of repeating a full 2D kernel.
    vec4 cx = abs(taps[0]+taps[1]-2.0*center);
    vec4 cy = abs(taps[2]+taps[3]-2.0*center);
    float ex = dot(cx,vec4(0.2126,0.7152,0.0722,1.0));
    float ey = dot(cy,vec4(0.2126,0.7152,0.0722,1.0));
    return mix(horizontal,vertical,ey/max(ex+ey,1.0e-6));
}

vec3 windshieldReflection(vec3 fallback, vec3 n, vec3 v, float roughness, vec3 f0, float weight, vec3 ng)
{
    bool inside = u_HasInteriorVolume == 0 ||
        (all(greaterThanEqual(v_Position, u_InteriorMin)) && all(lessThanEqual(v_Position, u_InteriorMax)));
    fallback *= inside ? u_SsrOptions.z : 1.0;
    if (u_SsrOptions.x == 0.0 || u_SsrAttenuation <= 0.0 || u_AsoboGlassCopyValid == 0) return fallback;
    vec4 filtered = smoothWindshield(n,v,ng,roughness);
    vec2 ab = texture(u_GGXLUT, vec2(clampedDot(n, v), roughness)).rg;
    vec3 ks = f0 + (max(vec3(1.0 - roughness), f0) - f0) * pow(1.0 - clampedDot(n, v), 5.0);
    vec3 reflected = filtered.rgb * weight * (ks * ab.x + ab.y);
    float attenuation = clamp(u_SsrAttenuation,0.0,1.0);
    return fallback * (1.0-filtered.a*attenuation) + reflected*attenuation;
}
#endif

void main()
{
#ifdef ASOBO_DECAL_RECEIVER
    DecalReceiverInfo decalReceiver = getDecalReceiverInfo();
#endif

    vec4 baseColor = getBaseColor();
#ifdef ASOBO_DECAL_RECEIVER
    baseColor.rgb = applyDecalColor(baseColor.rgb, decalReceiver.baseColor);
#endif

#if ALPHAMODE == ALPHAMODE_OPAQUE
    baseColor.a = 1.0;
#endif

    vec3 v = normalize(u_Camera - v_Position);
    NormalInfo normalInfo = getNormalInfo(v);
#ifdef ASOBO_DECAL_RECEIVER
    // A2.2: the decal normal is world space; mix it with the receiver's shading normal
    // and renormalize before any NdotV term is derived. The explicit branch keeps a
    // receiver with T == 1 bit-for-bit unchanged.
    if (decalReceiver.normal.a < 1.0) {
        normalInfo.n = normalize(decalReceiver.normal.rgb + decalReceiver.normal.a * normalInfo.n);
    }
#endif
    vec3 n = normalInfo.n;
    vec3 t = normalInfo.t;
    vec3 b = normalInfo.b;

    float NdotV = clampedDot(n, v);
    float TdotV = clampedDot(t, v);
    float BdotV = clampedDot(b, v);

    MaterialInfo materialInfo;
    materialInfo.baseColor = baseColor.rgb;
    
    // The default index of refraction of 1.5 yields a dielectric normal incidence reflectance of 0.04.
    materialInfo.ior = 1.5;
    materialInfo.f0 = vec3(0.04);
    materialInfo.specularWeight = 1.0;

    // If the MR debug output is selected, we have to enforce evaluation of the non-iridescence BRDF functions.
#if DEBUG == DEBUG_METALLIC_ROUGHNESS
#undef MATERIAL_IRIDESCENCE
#endif

#ifdef MATERIAL_IOR
    materialInfo = getIorInfo(materialInfo);
#endif

#ifdef MATERIAL_SPECULARGLOSSINESS
    materialInfo = getSpecularGlossinessInfo(materialInfo);
#endif

#ifdef MATERIAL_METALLICROUGHNESS
    materialInfo = getMetallicRoughnessInfo(materialInfo);
#endif

#ifdef MATERIAL_SHEEN
    materialInfo = getSheenInfo(materialInfo);
#endif

#ifdef MATERIAL_CLEARCOAT
    materialInfo = getClearCoatInfo(materialInfo, normalInfo);
#endif

#ifdef MATERIAL_SPECULAR
    materialInfo = getSpecularInfo(materialInfo);
#endif

#ifdef MATERIAL_TRANSMISSION
    materialInfo = getTransmissionInfo(materialInfo);
#endif

#ifdef MATERIAL_VOLUME
    materialInfo = getVolumeInfo(materialInfo);
#endif

#ifdef MATERIAL_IRIDESCENCE
    materialInfo = getIridescenceInfo(materialInfo);
#endif

    materialInfo.perceptualRoughness = clamp(materialInfo.perceptualRoughness, 0.0, 1.0);
    materialInfo.metallic = clamp(materialInfo.metallic, 0.0, 1.0);

    // Roughness is authored as perceptual roughness; as is convention,
    // convert to material roughness by squaring the perceptual roughness.
    materialInfo.alphaRoughness = materialInfo.perceptualRoughness * materialInfo.perceptualRoughness;

    // Compute reflectance.
    float reflectance = max(max(materialInfo.f0.r, materialInfo.f0.g), materialInfo.f0.b);

    // Anything less than 2% is physically impossible and is instead considered to be shadowing. Compare to "Real-Time-Rendering" 4th editon on page 325.
    materialInfo.f90 = vec3(1.0);

    // LIGHTING
    vec3 f_specular = vec3(0.0);
    vec3 f_diffuse = vec3(0.0);
    vec3 f_emissive = vec3(0.0);
    vec3 f_clearcoat = vec3(0.0);
    vec3 f_sheen = vec3(0.0);
    vec3 f_transmission = vec3(0.0);

    float albedoSheenScaling = 1.0;

#ifdef MATERIAL_IRIDESCENCE
    vec3 iridescenceFresnel = materialInfo.f0;
    vec3 iridescenceF0 = materialInfo.f0;

    if (materialInfo.iridescenceThickness == 0.0) {
        materialInfo.iridescenceFactor = 0.0;
    }

    if (materialInfo.iridescenceFactor > 0.0) {
        iridescenceFresnel = evalIridescence(1.0, materialInfo.iridescenceIor, NdotV, materialInfo.iridescenceThickness, materialInfo.f0);
        iridescenceF0 = Schlick_to_F0(iridescenceFresnel, NdotV);
    }
#endif

    // Calculate lighting contribution from image based lighting source (IBL)
#ifdef USE_IBL
#ifdef MATERIAL_IRIDESCENCE
    f_specular += getIBLRadianceGGXIridescence(n, v, materialInfo.perceptualRoughness, materialInfo.f0, iridescenceFresnel, materialInfo.iridescenceFactor, materialInfo.specularWeight);
    f_diffuse += getIBLRadianceLambertianIridescence(n, v, materialInfo.perceptualRoughness, materialInfo.c_diff, materialInfo.f0, iridescenceF0, materialInfo.iridescenceFactor, materialInfo.specularWeight);
#else
#ifdef ASOBO_WINDSHIELD_SSR
    f_specular += windshieldReflection(getIBLRadianceGGX(n, v, materialInfo.perceptualRoughness, materialInfo.f0, materialInfo.specularWeight),
        n, v, materialInfo.perceptualRoughness, materialInfo.f0, materialInfo.specularWeight, normalInfo.ng);
#else
    f_specular += getIBLRadianceGGX(n, v, materialInfo.perceptualRoughness, materialInfo.f0, materialInfo.specularWeight);
#endif
    f_diffuse += getIBLRadianceLambertian(n, v, materialInfo.perceptualRoughness, materialInfo.c_diff, materialInfo.f0, materialInfo.specularWeight);
#endif

#ifdef MATERIAL_CLEARCOAT
    f_clearcoat += getIBLRadianceGGX(materialInfo.clearcoatNormal, v, materialInfo.clearcoatRoughness, materialInfo.clearcoatF0, 1.0);
#endif

#ifdef MATERIAL_SHEEN
    f_sheen += getIBLRadianceCharlie(n, v, materialInfo.sheenRoughnessFactor, materialInfo.sheenColorFactor);
#endif
#endif

#if defined(ASOBO_WINDSHIELD_SSR) && !defined(USE_IBL)
    f_specular += windshieldReflection(vec3(0), n, v, materialInfo.perceptualRoughness, materialInfo.f0, materialInfo.specularWeight, normalInfo.ng);
#endif
#if defined(MATERIAL_TRANSMISSION) && defined(USE_IBL)
    f_transmission += getIBLVolumeRefraction(
        n, v,
        materialInfo.perceptualRoughness,
        materialInfo.c_diff, materialInfo.f0, materialInfo.f90,
        v_Position, u_ModelMatrix, u_ViewMatrix, u_ProjectionMatrix,
        materialInfo.ior, materialInfo.thickness, materialInfo.attenuationColor, materialInfo.attenuationDistance);
#endif

    float ao = 1.0;
    // Apply optional PBR terms for additional (optional) shading
#ifdef HAS_OCCLUSION_MAP
    ao = texture(u_OcclusionSampler,  getOcclusionUV()).r;
#ifdef ASOBO_ASSET_OPTIMIZED
    // A1.12: the packaged COMP texture stores AO as sqrt(linear AO).
    ao *= ao;
#endif
#ifdef ASOBO_MATERIAL_DETAIL_MAP
    // A5.1: the detail OMR red channel offsets the material occlusion; the exporter applies
    // it in the same occlusion path, so it needs a material occlusion target to act on.
    ao = applyAsoboDetailOcclusion(getAsoboDetailWeight(), ao);
#endif
    float occlusionStrength = u_OcclusionStrength;
#ifdef ASOBO_OCCLUSION_STRENGTH
    // A1.5: the MSFS exporter writes the material occlusion strength only as
    // ASOBO_occlusion_strength (never occlusionTexture.strength), so it takes the core
    // strength's place with the core operator (inferred). Strengths above 1 deepen the
    // occlusion; the factor is clamped at 0 below.
    if (u_AsoboOcclusionStrengthApplied != 0) {
        occlusionStrength = u_AsoboOcclusionStrength;
    }
#endif
    f_diffuse = mix(f_diffuse, f_diffuse * ao, occlusionStrength);
    // apply ambient occlusion to all lighting that is not punctual
    f_specular = mix(f_specular, f_specular * ao, occlusionStrength);
    f_sheen = mix(f_sheen, f_sheen * ao, occlusionStrength);
    f_clearcoat = mix(f_clearcoat, f_clearcoat * ao, occlusionStrength);
#ifdef ASOBO_OCCLUSION_STRENGTH
    f_diffuse = max(f_diffuse, vec3(0.0));
    f_specular = max(f_specular, vec3(0.0));
    f_sheen = max(f_sheen, vec3(0.0));
    f_clearcoat = max(f_clearcoat, vec3(0.0));
#endif
#endif
#ifdef ASOBO_DECAL_RECEIVER
    // A2.2: the decal AO multiplies the IBL terms with strength 1, also when the receiver
    // has no occlusion map. The debug Occlusion view shows the combined product.
    if (decalReceiver.occlusion < 1.0) {
        ao *= decalReceiver.occlusion;
        f_diffuse *= decalReceiver.occlusion;
        f_specular *= decalReceiver.occlusion;
        f_sheen *= decalReceiver.occlusion;
        f_clearcoat *= decalReceiver.occlusion;
    }
#endif

#ifdef USE_PUNCTUAL
    for (int i = 0; i < LIGHT_COUNT; ++i)
    {
        Light light = u_Lights[i];

        vec3 pointToLight;
        if (light.type != LightType_Directional)
        {
            pointToLight = light.position - v_Position;
        }
        else
        {
            pointToLight = -light.direction;
        }

        // BSTF
        vec3 l = normalize(pointToLight);   // Direction from surface point to light
        vec3 h = normalize(l + v);          // Direction of the vector between l and v, called halfway vector
        float NdotL = clampedDot(n, l);
        float NdotV = clampedDot(n, v);
        float NdotH = clampedDot(n, h);
        float LdotH = clampedDot(l, h);
        float VdotH = clampedDot(v, h);
        if (NdotL > 0.0 || NdotV > 0.0)
        {
            // Calculation of analytical light
            // https://github.com/KhronosGroup/glTF/tree/master/specification/2.0#acknowledgments AppendixB
            vec3 intensity = getLighIntensity(light, pointToLight);
#ifdef MATERIAL_IRIDESCENCE
            f_diffuse += intensity * NdotL *  BRDF_lambertianIridescence(materialInfo.f0, materialInfo.f90, iridescenceFresnel, materialInfo.iridescenceFactor, materialInfo.c_diff, materialInfo.specularWeight, VdotH);
            f_specular += intensity * NdotL * BRDF_specularGGXIridescence(materialInfo.f0, materialInfo.f90, iridescenceFresnel, materialInfo.alphaRoughness, materialInfo.iridescenceFactor, materialInfo.specularWeight, VdotH, NdotL, NdotV, NdotH);
#else
            f_diffuse += intensity * NdotL *  BRDF_lambertian(materialInfo.f0, materialInfo.f90, materialInfo.c_diff, materialInfo.specularWeight, VdotH);
            f_specular += intensity * NdotL * BRDF_specularGGX(materialInfo.f0, materialInfo.f90, materialInfo.alphaRoughness, materialInfo.specularWeight, VdotH, NdotL, NdotV, NdotH);
#endif

#ifdef MATERIAL_SHEEN
            f_sheen += intensity * getPunctualRadianceSheen(materialInfo.sheenColorFactor, materialInfo.sheenRoughnessFactor, NdotL, NdotV, NdotH);
            albedoSheenScaling = min(1.0 - max3(materialInfo.sheenColorFactor) * albedoSheenScalingLUT(NdotV, materialInfo.sheenRoughnessFactor),
                1.0 - max3(materialInfo.sheenColorFactor) * albedoSheenScalingLUT(NdotL, materialInfo.sheenRoughnessFactor));
#endif

#ifdef MATERIAL_CLEARCOAT
            f_clearcoat += intensity * getPunctualRadianceClearCoat(materialInfo.clearcoatNormal, v, l, h, VdotH,
                materialInfo.clearcoatF0, materialInfo.clearcoatF90, materialInfo.clearcoatRoughness);
#endif
        }

        // BDTF
#ifdef MATERIAL_TRANSMISSION
        // If the light ray travels through the geometry, use the point it exits the geometry again.
        // That will change the angle to the light source, if the material refracts the light ray.
        vec3 transmissionRay = getVolumeTransmissionRay(n, v, materialInfo.thickness, materialInfo.ior, u_ModelMatrix);
        pointToLight -= transmissionRay;
        l = normalize(pointToLight);

        vec3 intensity = getLighIntensity(light, pointToLight);
        vec3 transmittedLight = intensity * getPunctualRadianceTransmission(n, v, l, materialInfo.alphaRoughness, materialInfo.f0, materialInfo.f90, materialInfo.c_diff, materialInfo.ior);

#ifdef MATERIAL_VOLUME
        transmittedLight = applyVolumeAttenuation(transmittedLight, length(transmissionRay), materialInfo.attenuationColor, materialInfo.attenuationDistance);
#endif

        f_transmission += transmittedLight;
#endif
    }
#endif

    f_emissive = u_EmissiveFactor;
    if (u_AsoboEmissiveActive != 0) {
        f_emissive *= u_MsfsEmissiveScale;
        if (u_AsoboMaterialEmissiveApplied != 0) {
            float dayNightMult = (u_TimeOfDay == 1) ? u_EmissiveNightMultiplier : u_EmissiveDayMultiplier;
            f_emissive *= dayNightMult;
        }
    } else {
        f_emissive = clamp(f_emissive, vec3(0.0), vec3(1.0));
    }
#ifdef MATERIAL_EMISSIVE_STRENGTH
    f_emissive *= u_EmissiveStrength;
#endif
#ifdef HAS_EMISSIVE_MAP
    f_emissive *= texture(u_EmissiveSampler, getEmissiveUV()).rgb;
#endif
#ifdef ASOBO_DECAL_RECEIVER
    if (decalReceiver.emissive.a < 1.0) {
        f_emissive = decalReceiver.emissive.rgb + decalReceiver.emissive.a * f_emissive;
    }
#endif
#ifdef ASOBO_MATERIAL_WINDSHIELD
    f_emissive = vec3(0.0);
#endif

    // Layer blending

    float clearcoatFactor = 0.0;
    vec3 clearcoatFresnel = vec3(0);

#ifdef MATERIAL_CLEARCOAT
    clearcoatFactor = materialInfo.clearcoatFactor;
    clearcoatFresnel = F_Schlick(materialInfo.clearcoatF0, materialInfo.clearcoatF90, clampedDot(materialInfo.clearcoatNormal, v));
    f_clearcoat = f_clearcoat * clearcoatFactor;
#endif

#ifdef MATERIAL_TRANSMISSION
    vec3 diffuse = mix(f_diffuse, f_transmission, materialInfo.transmissionFactor);
#else
    vec3 diffuse = f_diffuse;
#endif

    vec3 color = vec3(0);
#ifdef MATERIAL_UNLIT
    color = baseColor.rgb;
#else
    color = f_emissive + diffuse + f_specular;
    color = f_sheen + color * albedoSheenScaling;
    color = color * (1.0 - clearcoatFactor * clearcoatFresnel) + f_clearcoat;
#endif

#if DEBUG == DEBUG_NONE || DEBUG == DEBUG_SSR || DEBUG == DEBUG_SSR_RAY

#if ALPHAMODE == ALPHAMODE_MASK
    // Late discard to avoid samplig artifacts. See https://github.com/KhronosGroup/glTF-Sample-Viewer/issues/267
    if (baseColor.a < u_AlphaCutoff)
    {
        discard;
    }
    baseColor.a = 1.0;
#endif

#ifdef ASOBO_GLASS_REFLECTION
    // Existing GGX IBL and punctual terms already include Fresnel. Coverage applies
    // only to the base layer, never to the reflection. Keep debug views unchanged.
    float coverage = clamp(baseColor.a, 0.0, 1.0);
#ifdef ASOBO_GLASS_LEGACY
    // MSFS 2020's baked cockpit reflection is approximated by Glider's GGX IBL.
    // Its deformation factor is not a physical slab width.
    f_specular *= u_AsoboGlassReflectionMask;
#endif
#ifdef MATERIAL_UNLIT
    color = baseColor.rgb * coverage;
#else
    color = (f_emissive + diffuse) * coverage + f_specular;
    color = f_sheen + color * albedoSheenScaling;
    color = color * (1.0 - clearcoatFactor * clearcoatFresnel) + f_clearcoat;
#endif
#ifdef ASOBO_MATERIAL_WINDSHIELD
#ifdef MATERIAL_UNLIT
    color += getWindshieldDetailScattering(baseColor.rgb);
#else
    color += getWindshieldDetailScattering(diffuse);
#endif
#endif
#ifdef ASOBO_GLASS_REFRACTION
    if (u_AsoboGlassCopyValid != 0) {
        color += getAsoboGlassBehind(n, v, materialInfo.ior, materialInfo.perceptualRoughness) * (1.0 - coverage) * baseColor.rgb;
        // This is the complete scene colour, not a translucent layer. ImGui must
        // not multiply it by glass coverage again when it displays the viewport.
        baseColor.a = 1.0;
    }
#endif
#endif

#ifdef LINEAR_OUTPUT
    g_finalColor = vec4(fitHalfFloatHdr(color.rgb), baseColor.a);
#else
    g_finalColor = vec4(toneMap(color), baseColor.a);
#endif

#else
    // In case of missing data for a debug view, render a checkerboard.
    g_finalColor = vec4(1.0);
    {
        float frequency = 0.02;
        float gray = 0.9;

        vec2 v1 = step(0.5, fract(frequency * gl_FragCoord.xy));
        vec2 v2 = step(0.5, vec2(1.0) - fract(frequency * gl_FragCoord.xy));
        g_finalColor.rgb *= gray + v1.x * v1.y + v2.x * v2.y;
    }
#endif

    // Debug views:

    // Generic:
#if DEBUG == DEBUG_UV_0 && defined(HAS_TEXCOORD_0_VEC2)
    g_finalColor.rgb = vec3(v_texcoord_0, 0);
#endif
#if DEBUG == DEBUG_UV_1 && defined(HAS_TEXCOORD_1_VEC2)
    g_finalColor.rgb = vec3(v_texcoord_1, 0);
#endif
#if DEBUG == DEBUG_NORMAL_TEXTURE && defined(HAS_NORMAL_MAP)
    g_finalColor.rgb = (normalInfo.ntex + 1.0) / 2.0;
#endif
#if DEBUG == DEBUG_NORMAL_SHADING
    g_finalColor.rgb = (n + 1.0) / 2.0;
#endif
#if DEBUG == DEBUG_NORMAL_GEOMETRY
    g_finalColor.rgb = (normalInfo.ng + 1.0) / 2.0;
#endif
#if DEBUG == DEBUG_TANGENT
    g_finalColor.rgb = (normalInfo.t + 1.0) / 2.0;
#endif
#if DEBUG == DEBUG_BITANGENT
    g_finalColor.rgb = (normalInfo.b + 1.0) / 2.0;
#endif
#if DEBUG == DEBUG_ALPHA
    g_finalColor.rgb = linearTosRGB(vec3(baseColor.a));
#endif
#if DEBUG == DEBUG_OCCLUSION && defined(HAS_OCCLUSION_MAP)
    g_finalColor.rgb = linearTosRGB(vec3(ao));
#elif DEBUG == DEBUG_OCCLUSION && defined(ASOBO_DECAL_RECEIVER)
    // A2.2: show the decal AO product even when the receiver has no occlusion map.
    if (u_AsoboDecalReceiver != 0) {
        g_finalColor.rgb = linearTosRGB(vec3(ao));
    }
#endif
#if DEBUG == DEBUG_EMISSIVE
    g_finalColor.rgb = linearTosRGB(f_emissive);
#endif

    // MR:
#ifdef MATERIAL_METALLICROUGHNESS
#if DEBUG == DEBUG_METALLIC_ROUGHNESS
    g_finalColor.rgb = linearTosRGB(f_diffuse + f_specular);
#endif
#if DEBUG == DEBUG_METALLIC
    g_finalColor.rgb = linearTosRGB(vec3(materialInfo.metallic));
#endif
#if DEBUG == DEBUG_ROUGHNESS
    g_finalColor.rgb = linearTosRGB(vec3(materialInfo.perceptualRoughness));
#endif
#if DEBUG == DEBUG_BASE_COLOR
    g_finalColor.rgb = linearTosRGB(materialInfo.baseColor);
#endif
#endif

    // Clearcoat:
#ifdef MATERIAL_CLEARCOAT
#if DEBUG == DEBUG_CLEARCOAT
    g_finalColor.rgb = linearTosRGB(f_clearcoat);
#endif
#if DEBUG == DEBUG_CLEARCOAT_FACTOR
    g_finalColor.rgb = linearTosRGB(vec3(materialInfo.clearcoatFactor));
#endif
#if DEBUG == DEBUG_CLEARCOAT_ROUGHNESS
    g_finalColor.rgb = linearTosRGB(vec3(materialInfo.clearcoatRoughness));
#endif
#if DEBUG == DEBUG_CLEARCOAT_NORMAL
    g_finalColor.rgb = (materialInfo.clearcoatNormal + vec3(1)) / 2.0;
#endif
#endif

    // Sheen:
#ifdef MATERIAL_SHEEN
#if DEBUG == DEBUG_SHEEN
    g_finalColor.rgb = linearTosRGB(f_sheen);
#endif
#if DEBUG == DEBUG_SHEEN_COLOR
    g_finalColor.rgb = linearTosRGB(materialInfo.sheenColorFactor);
#endif
#if DEBUG == DEBUG_SHEEN_ROUGHNESS
    g_finalColor.rgb = linearTosRGB(vec3(materialInfo.sheenRoughnessFactor));
#endif
#endif

    // Specular:
#ifdef MATERIAL_SPECULAR
#if DEBUG == DEBUG_SPECULAR
    g_finalColor.rgb = linearTosRGB(f_specular);
#endif
#if DEBUG == DEBUG_SPECULAR_FACTOR
    g_finalColor.rgb = vec3(materialInfo.specularWeight);
#endif

#if DEBUG == DEBUG_SPECULAR_COLOR
vec3 specularTexture = vec3(1.0);
#ifdef HAS_SPECULAR_COLOR_MAP
    specularTexture.rgb = texture(u_SpecularColorSampler, getSpecularColorUV()).rgb;
#endif
    g_finalColor.rgb = u_KHR_materials_specular_specularColorFactor * specularTexture.rgb;
#endif
#endif

    // Transmission, Volume:
#ifdef MATERIAL_TRANSMISSION
#if DEBUG == DEBUG_TRANSMISSION_VOLUME
    g_finalColor.rgb = linearTosRGB(f_transmission);
#endif
#if DEBUG == DEBUG_TRANSMISSION_FACTOR
    g_finalColor.rgb = linearTosRGB(vec3(materialInfo.transmissionFactor));
#endif
#endif
#ifdef MATERIAL_VOLUME
#if DEBUG == DEBUG_VOLUME_THICKNESS
    g_finalColor.rgb = linearTosRGB(vec3(materialInfo.thickness));
#endif
#endif

    // Iridescence:
#ifdef MATERIAL_IRIDESCENCE
#if DEBUG == DEBUG_IRIDESCENCE
    g_finalColor.rgb = linearTosRGB(iridescenceFresnel);
#endif
#if DEBUG == DEBUG_IRIDESCENCE_FACTOR
    g_finalColor.rgb = linearTosRGB(vec3(materialInfo.iridescenceFactor));
#endif
#if DEBUG == DEBUG_IRIDESCENCE_THICKNESS
    g_finalColor.rgb = linearTosRGB(vec3(materialInfo.iridescenceThickness / 1200.0));
#endif
#endif
#if DEBUG == DEBUG_SSR && defined(ASOBO_WINDSHIELD_SSR)
    vec4 ssrDebug = traceWindshield(n, v, normalInfo.ng, v_Position);
    g_finalColor = vec4(ssrDebug.xy, ssrDebug.a, 2.0); // R/G hit UV; B confidence; black miss
#endif
#if DEBUG == DEBUG_SSR_RAY && defined(ASOBO_WINDSHIELD_SSR)
    vec3 ng = dot(normalInfo.ng,v) >= 0.0 ? normalInfo.ng : -normalInfo.ng;
    vec3 reflected = reflect(-v,n);
    g_finalColor = vec4(normalize(mat3(u_ViewMatrix)*reflected)*0.5+0.5, 2.0);
    if (dot(reflected,ng) <= 0.02) g_finalColor = vec4(1,0,1,2); // invalid pane hemisphere
#endif

}
