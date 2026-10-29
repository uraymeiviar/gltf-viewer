// A3.6: ground plane (docs/ASOBO_EXTENSIONS_PHASE_3.md). An infinite horizontal plane (glTF Y-up) at
// u_GroundHeight, intersected per pixel. It writes the plane's depth (clamped to the far plane), so the model
// occludes it and it occludes what is below it, and it is a simple PBR surface lit by the IBL (diffuse and
// specular) and by the A3.1 sun with the A3.2 shadow lookup. The lower hemisphere of the lighting is the
// HDR's own. Distance fog fades the plane into the environment's horizon colour, so its edge is no line.
//
// Includes only what it needs (the shader cache hoists every include directive above this text, so the shared constants
// and the environment samplers live in ground_common.glsl; the CommandGenerator binds the samplers when an
// environment exists, GROUND_ENV, and the shadow map comes with shadow.glsl).
#include <camera_block.glsl>
#include <environment_block.glsl>

#include <ground_common.glsl>
#include <brdf.glsl>
#include <ibl.glsl>
#include <shadow.glsl>

in vec3 v_NearPoint;
in vec3 v_FarPoint;
out vec4 g_finalColor;

uniform float u_GroundHeight;     // world Y of the plane
uniform vec3 u_GroundAlbedo;      // linear base colour (the average albedo A6.2 reads)
uniform float u_GroundRoughness;  // perceptual roughness
uniform vec3 u_GroundDetail;      // x: amount of albedo variation 0..1, y: size of the largest feature (scene units), z: slab joint spacing (0 = none)
// x: fog extinction per scene unit (fog = 1 - exp(-x distance)), y: 1 = the fog colour is the environment's
// horizon, 0 = u_GroundHazeColor (the clear colour, while the background is off).
uniform vec2 u_GroundHaze;
uniform vec3 u_GroundHazeColor;

float hash12(vec2 p)
{
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

float valueNoise(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash12(i), hash12(i + vec2(1.0, 0.0)), u.x),
               mix(hash12(i + vec2(0.0, 1.0)), hash12(i + vec2(1.0, 1.0)), u.x), u.y);
}

// Albedo multiplier with mean 1: four octaves of value noise at fixed world positions (nothing moves with
// the camera or time, so TAA only has to integrate sub-pixel samples). An octave fades out, to its mean,
// as the pixel footprint approaches half its wavelength, so distant ground shows the average and neither
// shimmers nor aliases.
float groundDetailFactor(vec2 position, float footprint)
{
    float amount = u_GroundDetail.x;
    if (amount <= 0.0) {
        return 1.0;
    }
    const float wavelengths[4] = float[4](8.0, 2.0, 0.5, 0.125);
    const float weights[4] = float[4](0.20, 0.30, 0.30, 0.20);
    float variation = 0.0;
    for (int i = 0; i < 4; ++i) {
        float wavelength = wavelengths[i] * u_GroundDetail.y;
        float fade = 1.0 - smoothstep(0.1, 0.5, footprint / wavelength);
        // The octave offsets keep the lattices from lining up at the origin.
        variation += weights[i] * fade * (2.0 * valueNoise(position / wavelength + float(i) * 17.31) - 1.0);
    }
    return max(1.0 + amount * 1.2 * variation, 0.0);
}

// Slab joints: thin dark lines every u_GroundDetail.z units in X and Z, filtered over the pixel footprint (a
// line narrower than a pixel fades to its average darkening instead of aliasing). Returns the albedo multiplier.
float groundJointFactor(vec2 position, float footprint)
{
    float spacing = u_GroundDetail.z;
    if (spacing <= 0.0) {
        return 1.0;
    }
    float halfWidth = 0.004 * spacing;
    vec2 distanceToJoint = abs(fract(position / spacing + 0.5) - 0.5) * spacing;
    float nearest = min(distanceToJoint.x, distanceToJoint.y);
    float width = max(footprint, 1.0e-6);
    // Box-filtered line coverage, scaled down when the line is narrower than the footprint.
    float coverage = clamp((halfWidth - nearest) / width + 0.5, 0.0, 1.0) * min(1.0, 2.0 * halfWidth / width);
    // Two lines can fall in one pixel only when it is wider than the spacing: fade them to their mean.
    coverage = mix(coverage, 4.0 * halfWidth / spacing, smoothstep(0.1 * spacing, 0.5 * spacing, footprint));
    return 1.0 - 0.55 * coverage;
}

void main()
{
    vec3 d = v_FarPoint - v_NearPoint;
    if (d.y == 0.0) {
        discard;
    }
    float t = (u_GroundHeight - v_NearPoint.y) / d.y;
    if (t <= 0.0) {
        discard; // the plane is behind this pixel's ray (looking up from above, down from below)
    }

    vec3 rayDirection = normalize(d);
    vec3 hazeDirection = normalize(vec3(rayDirection.x, 0.0, rayDirection.z) + vec3(0.0, 0.004, 0.0));
    // Fog colour: the environment just above the horizon in this ray's azimuth, one mip softer than the background
    // is drawn (so the plane meets the sky it fades into), or the clear colour while the background is off.
    vec3 hazeColor = u_GroundHazeColor;
#ifdef GROUND_ENV
    if (u_GroundHaze.y > 0.5) {
        float lod = u_EnvBlurNormalized * float(u_MipCount - 1) + 1.0;
        hazeColor = getSpecularSample(hazeDirection, lod).rgb;
    }
#endif

    vec3 position = v_NearPoint + t * d;
    // Beyond the far plane (t > 1) the plane is still shaded (it is fully fogged long before) at depth 1, the
    // depth the sky has.
    vec4 clip = u_ViewProjectionMatrix * vec4(position, 1.0);
    gl_FragDepth = t > 1.0 ? 1.0 : clamp(clip.z / clip.w * 0.5 + 0.5, 0.0, 1.0);

    vec3 toCamera = u_Camera - position;
    float distanceToCamera = length(toCamera);
    vec3 v = toCamera / max(distanceToCamera, 1.0e-6);
    // Two-sided: seen from below the plane shows the same surface, lit as if facing the camera.
    vec3 n = vec3(0.0, u_Camera.y >= u_GroundHeight ? 1.0 : -1.0, 0.0);

    float footprint = max(length(dFdx(position.xz)), length(dFdy(position.xz)));
    vec3 baseColor = u_GroundAlbedo * groundDetailFactor(position.xz, footprint) * groundJointFactor(position.xz, footprint);

    // Dielectric (F0 0.04) with the glTF metallic-roughness conventions of pbr.frag.
    float perceptualRoughness = clamp(u_GroundRoughness, 0.04, 1.0);
    float alphaRoughness = perceptualRoughness * perceptualRoughness;
    vec3 f0 = vec3(0.04);
    vec3 f90 = vec3(1.0);
    vec3 diffuseColor = baseColor;

    vec3 f_diffuse = vec3(0.0);
    vec3 f_specular = vec3(0.0);
#ifdef GROUND_ENV
#ifdef USE_IBL
    f_diffuse += getIBLRadianceLambertian(n, v, perceptualRoughness, diffuseColor, f0, 1.0);
    f_specular += getIBLRadianceGGX(n, v, perceptualRoughness, f0, 1.0);
#endif
#endif
    // A3.8 viewer ambient light: the same uniform radiance as pbr.frag (Lambert and, with the environment's BRDF LUT,
    // L x (F0 A + B)); the plane has no occlusion.
    if (u_AmbientEnabled != 0) {
        f_diffuse += diffuseColor * u_AmbientRadiance;
#ifdef GROUND_ENV
        vec2 ambientAB = texture(u_GGXLUT, clamp(vec2(clampedDot(n, v), perceptualRoughness), vec2(0.0), vec2(1.0))).rg;
        f_specular += u_AmbientRadiance * (f0 * ambientAB.x + ambientAB.y);
#endif
    }

    if (u_SunEnabled != 0) {
        // A3.2: the same lookup as pbr.frag (visibility 1 while shadows are off).
        float sunVisibility = sunShadowVisibility(position, n);
        vec3 sunIntensity = u_SunIlluminance * sunVisibility;
        float sunWiden = 0.5 * tan(u_SunAngularRadius);
        vec3 sunL = normalize(u_SunDirection);
        vec3 sunH = normalize(sunL + v);
        float sunNdotL = clampedDot(n, sunL);
        float sunNdotV = clampedDot(n, v);
        float sunNdotH = clampedDot(n, sunH);
        float sunVdotH = clampedDot(v, sunH);
        float sunAlpha = min(alphaRoughness + sunWiden, 1.0);
        if (sunNdotL > 0.0 && sunNdotV > 0.0) {
            f_diffuse += sunIntensity * sunNdotL * BRDF_lambertian(f0, f90, diffuseColor, 1.0, sunVdotH);
            f_specular += sunIntensity * sunNdotL * BRDF_specularGGX(f0, f90, sunAlpha, 1.0, sunVdotH, sunNdotL, sunNdotV, sunNdotH);
        }
    }

    vec3 color = max(f_diffuse, vec3(0.0)) + max(f_specular, vec3(0.0));

    // Distance fog towards the horizon colour: 98% at the haze (visibility) distance, 100% at the horizon.
    float fog = 1.0 - exp(-u_GroundHaze.x * distanceToCamera);
    color = mix(color, hazeColor, fog);

    // A6.4: pre-exposure (environment_block.glsl), exactly 1 unless the procedural sky's physical exposure is on.
    if (u_PreExposure != 1.0) color *= u_PreExposure;
    g_finalColor = vec4(fitHalfFloatHdr(color), 1.0);
}
