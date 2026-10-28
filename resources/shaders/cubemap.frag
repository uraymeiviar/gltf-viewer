precision highp float;


#include <environment_block.glsl>
#include <tonemapping.glsl>


uniform samplerCube u_GGXEnvSampler;

#if defined(PROCEDURAL_SKY) || defined(SUN_DISK) || defined(MOON_DISK) || defined(STARS)
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

#ifdef MOON_DISK
// A6.6: the moon disk (NightSky.h evaluateMoon), background only. Lommel-Seeliger brightness mu0 / (mu0 + mu) of the
// sunlit half, normalised on the CPU so the disk integrates to the moon's illuminance (Krisciunas & Schaefer phase law,
// geometric albedo 0.12) through the atmosphere towards it; the terminator and the bright limb follow the true sun
// direction from the moon, so the phase and its orientation come out of the geometry.
uniform vec4 u_MoonDisk;        // xyz direction towards the moon (world, unit), w angular radius (radians)
uniform vec3 u_MoonSun;         // direction from the moon towards the sun (world, unit)
uniform vec3 u_MoonRadiance;    // K: radiance = K x mu0 / (mu0 + mu) (transmittance included)

// Returns the disk's radiance (rgb) and its coverage (a) at this pixel.
vec4 moonDisk(vec3 worldDirection)
{
    vec3 moon = u_MoonDisk.xyz;
    float angle = atan(length(cross(worldDirection, moon)), dot(worldDirection, moon));
    float radius = u_MoonDisk.w;
    float footprint = max(fwidth(angle), 1.0e-6);
    float coverage = clamp((radius - angle) / footprint + 0.5, 0.0, 1.0);
    if (coverage <= 0.0 || worldDirection.y < 0.0) return vec4(0.0);
    // The visible hemisphere's point under this pixel: q in the plane across the line of sight (|q| <= 1 at the limb).
    vec3 q = (worldDirection - dot(worldDirection, moon) * moon) / sin(radius);
    float q2 = min(dot(q, q), 1.0);
    float mu = sqrt(1.0 - q2);
    vec3 normal = q - mu * moon;
    float mu0 = dot(normal, normalize(u_MoonSun));
    float brightness = mu0 > 0.0 ? mu0 / (mu0 + max(mu, 1.0e-4)) : 0.0;
    return vec4(u_MoonRadiance * brightness, coverage);
}
#endif

#ifdef STARS
// A6.6: procedural stars (no catalogue): a cube-face grid on the celestial sphere, each cell holding a star with a
// probability proportional to its solid angle (u_Stars.x stars per steradian), at a hashed position, with a magnitude
// from N(<m) ~ 10^(0.5 m) down to the faintest magnitude and a hashed colour temperature. The equatorial frame turns
// with the local sidereal time (u_StarRotation: world to equatorial). A star is a Gaussian about one pixel wide that
// integrates to its illuminance (kStarMagnitudeZeroUnits x 10^(-0.4 m)), dimmed by the atmosphere's extinction for its
// airmass (Kasten & Young) and by u_Stars.y (the twilight fade). No twinkle. Background only: the stars' light reaches
// the IBL as part of the night sky glow (sky_passes.frag), not from here.
uniform mat3 u_StarRotation;
uniform vec4 u_Stars;           // x stars per steradian, y fade, z magnitude-0 illuminance, w faintest magnitude
uniform vec3 u_StarExtinction;  // optical depth towards the zenith (per channel)
const int kStarGrid = 256;      // cells per cube-face side

uint starHash(uint x)
{
    x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16;
    return x;
}

vec4 starRandom(int face, int i, int j)
{
    uint h = starHash(uint(face) * 0x9E3779B9u ^ starHash(uint(i) * 0x85EBCA6Bu ^ starHash(uint(j) + 0x27D4EB2Fu)));
    uint h2 = starHash(h ^ 0x165667B1u);
    uint h3 = starHash(h2 ^ 0xD3A2646Cu);
    uint h4 = starHash(h3 ^ 0xFD7046C5u);
    return vec4(float(h & 0xFFFFFFu), float(h2 & 0xFFFFFFu), float(h3 & 0xFFFFFFu), float(h4 & 0xFFFFFFu)) / 16777216.0;
}

vec3 starFaceDirection(int face, vec2 uv)
{
    if (face == 0) return vec3(1.0, uv.y, -uv.x);
    if (face == 1) return vec3(-1.0, uv.y, uv.x);
    if (face == 2) return vec3(uv.x, 1.0, -uv.y);
    if (face == 3) return vec3(uv.x, -1.0, uv.y);
    if (face == 4) return vec3(uv.x, uv.y, 1.0);
    return vec3(-uv.x, uv.y, -1.0);
}

vec3 stars(vec3 worldDirection)
{
    if (worldDirection.y <= 0.0) return vec3(0.0);
    vec3 c = u_StarRotation * worldDirection;
    vec3 a = abs(c);
    int face;
    vec2 uv;
    if (a.x >= a.y && a.x >= a.z) { face = c.x > 0.0 ? 0 : 1; uv = c.x > 0.0 ? vec2(-c.z, c.y) / a.x : vec2(c.z, c.y) / a.x; }
    else if (a.y >= a.z) { face = c.y > 0.0 ? 2 : 3; uv = c.y > 0.0 ? vec2(c.x, -c.z) / a.y : vec2(c.x, c.z) / a.y; }
    else { face = c.z > 0.0 ? 4 : 5; uv = c.z > 0.0 ? vec2(c.x, c.y) / a.z : vec2(-c.x, c.y) / a.z; }
    // Gaussian width: about the pixel's angular size (stable under TAA jitter).
    float pixel = max(length(fwidth(worldDirection)), 1.0e-5);
    float sigma = 0.6 * pixel;
    float cell = 2.0 / float(kStarGrid);
    vec2 g = (uv * 0.5 + 0.5) * float(kStarGrid);
    int ci = int(floor(g.x)), cj = int(floor(g.y));
    vec3 sum = vec3(0.0);
    for (int dj = -1; dj <= 1; ++dj) {
        for (int di = -1; di <= 1; ++di) {
            int i = ci + di, j = cj + dj;
            if (i < 0 || j < 0 || i >= kStarGrid || j >= kStarGrid) continue;
            vec2 centre = (vec2(float(i), float(j)) + 0.5) * cell - 1.0;
            // Solid angle of the cell: cell^2 / (1 + u^2 + v^2)^1.5.
            float r2 = 1.0 + dot(centre, centre);
            float probability = u_Stars.x * cell * cell / (r2 * sqrt(r2));
            vec4 h = starRandom(face, i, j);
            if (h.x >= probability) continue;
            vec2 p = centre + (h.yz - 0.5) * cell * 0.8;
            vec3 starCel = normalize(starFaceDirection(face, p));
            float d = length(starCel - c);
            if (d > 4.0 * sigma) continue;
            // Magnitude from the cumulative N(<m) ~ 10^(0.5 m): m = faintest + 2 log10(u), u = h.x / probability.
            float u = max(h.x / probability, 1.0e-6);
            float magnitude = max(u_Stars.w + 2.0 * log(u) / log(10.0), -1.5);
            float illuminance = u_Stars.z * pow(10.0, -0.4 * magnitude);
            // Colour: a hashed temperature between orange-white and blue-white, luminance 1.
            vec3 tint = mix(vec3(1.25, 0.95, 0.70), vec3(0.85, 0.98, 1.25), h.w);
            tint /= dot(tint, vec3(0.2126, 0.7152, 0.0722));
            sum += tint * illuminance * exp(-0.5 * d * d / (sigma * sigma)) / (2.0 * 3.14159265 * sigma * sigma);
        }
    }
    // Airmass (Kasten & Young 1989) of the line of sight and the extinction along it.
    float elevation = degrees(asin(clamp(worldDirection.y, 0.0, 1.0)));
    float airmass = 1.0 / (worldDirection.y + 0.50572 * pow(elevation + 6.07995, -1.6364));
    return sum * exp(-u_StarExtinction * airmass) * u_Stars.y;
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
#ifdef STARS
    vec3 starLight = stars(direction);
#endif
#ifdef MOON_DISK
    vec4 moon = moonDisk(direction);
    color.rgb += moon.rgb * moon.a;
#ifdef STARS
    starLight *= 1.0 - moon.a; // the moon covers the stars behind it
#endif
#endif
#ifdef STARS
    color.rgb += starLight;
#endif
#else
    vec4 color = textureLod(u_GGXEnvSampler, v_TexCoords, u_EnvBlurNormalized * float(u_MipCount - 1)) * u_EnvIntensity;
#endif
#ifdef SUN_DISK
    // v_TexCoords is u_EnvRotation x the cube position (identity rotation with the procedural sky).
    color.rgb += sunDisk(trueWorldDirection(transpose(u_EnvRotation) * v_TexCoords));
#endif

#ifdef PROCEDURAL_SKY
    // A6.4 pre-exposure (environment_block.glsl): exactly 1 unless the physical exposure modes run; kept inside the
    // half-float range.
    if (u_PreExposure != 1.0) color.rgb = min(color.rgb * u_PreExposure, vec3(64000.0));
#endif
#ifdef LINEAR_OUTPUT
    FragColor = color.rgba;
#else
    FragColor = vec4(toneMap(color.rgb), color.a);
#endif
}
