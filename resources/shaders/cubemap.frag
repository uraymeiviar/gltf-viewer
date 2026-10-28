precision highp float;


#include <environment_block.glsl>
#include <tonemapping.glsl>


uniform samplerCube u_GGXEnvSampler;

#if defined(PROCEDURAL_SKY) || defined(SUN_DISK)
#include <camera_block.glsl>
// cubemap.vert (from the Khronos sample viewer) projects the unit cube with w + 0.1 instead of w, so the background
// shows the direction p at the screen position of a slightly different direction (about 7% closer to the image
// centre). The HDR background keeps that projection (unchanged images); the procedural sky (A6.2) and the sun disk
// (A6.7) use the direction a perspective camera really sees at this fragment, so the disk sits exactly at the sun's
// projected position, in line with the geometry, the ground plane and the sun's highlights.
// In view space the drawn clip position is (P00 x + P20 z, P11 y + P21 z, ., -z + 0.1); the direction t with the same
// screen position under P is (x + 0.1 P20 / P00, y + 0.1 P21 / P11, z - 0.1) (P20 / P21: the TAA jitter).
vec3 trueWorldDirection(vec3 cubePosition)
{
    mat3 viewRotation = mat3(u_ViewMatrix);
    vec3 v = viewRotation * cubePosition;
    v += vec3(0.1 * u_ProjectionMatrix[2][0] / u_ProjectionMatrix[0][0], 0.1 * u_ProjectionMatrix[2][1] / u_ProjectionMatrix[1][1], -0.1);
    return normalize(transpose(viewRotation) * v);
}
#endif

#ifdef PROCEDURAL_SKY
#include <sky_common.glsl>
// A6.2: the procedural sky, drawn from the sky-view LUT every frame (so it follows the sun at once, independent of
// the time-sliced IBL cubes). v_TexCoords is the world direction: the environment rotation is identity in this mode.
uniform sampler2D u_SkyViewLUT;
uniform vec3 u_SkySunDirection; // world, unit, towards the sun
uniform vec4 u_SkyParams;       // x bottom radius, y top radius, z observer radius (km), w radiance scale
#endif

#ifdef SUN_DISK
// A6.7: the sun disk, added to the visible background only (this shader is never an IBL filter input; the sky's
// source cube comes from sky_passes.frag PASS_CUBE and the HDR's from the panorama). Parameters: SunDisk.h.
uniform vec4 u_SunDisk;          // x angular radius (radians, after the size scale), y limb darkening (0 / 1)
uniform vec3 u_SunDiskRadiance;  // mean disk radiance E / (pi sin^2 R), already scaled into the half-float range

// Hestroffer & Magnan (1998) limb darkening I(mu) / I(1) = mu^alpha at 680 / 550 / 440 nm, normalised so the disk
// mean stays u_SunDiskRadiance (the disk mean of mu^alpha is 2 / (alpha + 2)).
const vec3 kSunLimbAlpha = vec3(0.397, 0.503, 0.652);

vec3 sunDisk(vec3 worldDirection)
{
    vec3 sun = normalize(u_SunDirection);
    // The angle to the sun's centre, accurate near 0 (acos(dot) loses it below about 1e-3 rad in float).
    float angle = atan(length(cross(worldDirection, sun)), dot(worldDirection, sun));
    float radius = u_SunDisk.x;
    // Antialiased edge: coverage over the pixel's angular footprint, so the edge is about one pixel wide and the
    // same for every TAA jitter.
    float footprint = max(fwidth(angle), 1.0e-6);
    float coverage = clamp((radius - angle) / footprint + 0.5, 0.0, 1.0);
    if (coverage <= 0.0) return vec3(0.0);
    vec3 limb = vec3(1.0);
    if (u_SunDisk.y > 0.5) {
        float rho = min(angle / radius, 1.0);
        float mu = sqrt(max(1.0 - rho * rho, 0.0));
        limb = pow(vec3(max(mu, 1.0e-4)), kSunLimbAlpha) * (kSunLimbAlpha + 2.0) * 0.5;
    }
    return u_SunDiskRadiance * limb * coverage;
}
#endif

out vec4 FragColor;
in vec3 v_TexCoords;


void main()
{
#ifdef PROCEDURAL_SKY
    vec3 direction = trueWorldDirection(v_TexCoords);
    vec3 sky = skyViewRadiance(u_SkyViewLUT, direction, normalize(u_SkySunDirection), u_SkyParams.x, u_SkyParams.z) * u_SkyParams.w;
    // A6.7: the limb-darkened sun disk is added below (SUN_DISK), to the background only.
    // A6.6 hook: the moon disk and the stars (rotated by sidereal time) are added here too, attenuated by the
    // transmittance towards them; their contribution to the IBL goes through sky_passes.frag PASS_CUBE.
    vec4 color = vec4(sky, 1.0);
#else
    vec4 color = textureLod(u_GGXEnvSampler, v_TexCoords, u_EnvBlurNormalized * float(u_MipCount - 1)) * u_EnvIntensity;
#endif
#ifdef SUN_DISK
    // v_TexCoords is u_EnvRotation x the cube position (identity rotation with the procedural sky).
    color.rgb += sunDisk(trueWorldDirection(transpose(u_EnvRotation) * v_TexCoords));
#endif

#ifdef LINEAR_OUTPUT
    FragColor = color.rgba;
#else
    FragColor = vec4(toneMap(color.rgb), color.a);
#endif
}
