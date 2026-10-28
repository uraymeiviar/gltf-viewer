// A6.2 procedural sky passes (header/imgui/SkyEnvironment.h compiles this file once per pass define):
//   PASS_TRANSMITTANCE  256 x 64  transmittance to the top of the atmosphere (Bruneton 2017 parameterisation)
//   PASS_MULTISCATTER    32 x 32  Hillaire 2020 multiple-scattering term Psi_ms (sun zenith cosine, height)
//   PASS_SKYVIEW        192 x 108 radiance seen from the observer (sky_common.glsl mapping)
//   PASS_CUBE           one face of the environment cube from the sky-view LUT (IBL filter input)
// All radiance is per unit top-of-atmosphere sun illuminance (a white sun of illuminance 1); PASS_CUBE multiplies by
// u_RadianceScale, the solar illuminance in the environment's units. Distances in km, the planet centred at the
// origin, +Y the observer's zenith.

#include <sky_common.glsl>

out vec4 fragmentColor;

uniform vec4 u_AtmosphereRadii;   // x bottom radius, y top radius, z observer radius (km)
uniform vec4 u_Rayleigh;          // rgb Rayleigh scattering at sea level (1/km), a scale height (km)
uniform vec4 u_MieScattering;     // rgb Mie scattering at sea level (1/km), a scale height (km)
uniform vec4 u_MieExtinction;     // rgb Mie extinction at sea level (1/km), a phase asymmetry g
uniform vec4 u_OzoneAbsorption;   // rgb ozone absorption at the layer peak (1/km), a peak altitude (km)
uniform vec4 u_GroundAlbedo;      // rgb ground albedo, a ozone layer half width (km)
uniform vec3 u_SunDirection;      // PASS_SKYVIEW / PASS_CUBE: world, unit, towards the sun
uniform vec4 u_Airglow;           // A6.6: rgb night sky glow zenith radiance (per unit sun), a layer altitude (km)
uniform float u_RadianceScale;    // PASS_CUBE
uniform int u_Face;               // PASS_CUBE
uniform vec2 u_TargetSize;        // output size in texels
uniform sampler2D u_TransmittanceLUT;
uniform sampler2D u_MultiScatteringLUT;
uniform sampler2D u_SkyViewLUT;

const vec2 kTransmittanceLutSize = vec2(256.0, 64.0);
const float kMultiScatteringLutSize = 32.0;

float bottomRadius() { return u_AtmosphereRadii.x; }
float topRadius() { return u_AtmosphereRadii.y; }

struct Medium {
    vec3 rayleigh;
    vec3 mie;
    vec3 scattering;
    vec3 extinction;
};

Medium sampleMedium(float height)
{
    height = max(height, 0.0);
    float rayleighDensity = exp(-height / u_Rayleigh.a);
    float mieDensity = exp(-height / u_MieScattering.a);
    float ozoneDensity = max(0.0, 1.0 - abs(height - u_OzoneAbsorption.a) / u_GroundAlbedo.a);
    Medium medium;
    medium.rayleigh = u_Rayleigh.rgb * rayleighDensity;
    medium.mie = u_MieScattering.rgb * mieDensity;
    medium.scattering = medium.rayleigh + medium.mie;
    medium.extinction = medium.rayleigh + u_MieExtinction.rgb * mieDensity + u_OzoneAbsorption.rgb * ozoneDensity;
    return medium;
}

// Ray geometry from radius r with zenith cosine mu (Bruneton 2017, section 2).
float distanceToTop(float r, float mu)
{
    float top = topRadius();
    return max(-r * mu + skySafeSqrt(r * r * (mu * mu - 1.0) + top * top), 0.0);
}

bool rayHitsGround(float r, float mu)
{
    float h = max(r - bottomRadius(), 0.0);
    return mu < 0.0 && r * r * mu * mu - h * (2.0 * bottomRadius() + h) >= 0.0;
}

float distanceToBottom(float r, float mu)
{
    float h = max(r - bottomRadius(), 0.0);
    return max(-r * mu - skySafeSqrt(r * r * mu * mu - h * (2.0 * bottomRadius() + h)), 0.0);
}

// Height above the ground after distance t along the ray (r0, mu0), without cancellation near the ground.
float heightAlong(float r0, float mu0, float t, out float r)
{
    float h0 = max(r0 - bottomRadius(), 0.0);
    float r2MinusBottom2 = h0 * (2.0 * bottomRadius() + h0) + t * t + 2.0 * r0 * mu0 * t;
    r = skySafeSqrt(r0 * r0 + t * t + 2.0 * r0 * mu0 * t);
    float h = r2MinusBottom2 / (r + bottomRadius());
    return max(h, 0.0);
}

float unitToTexel(float x, float size) { return 0.5 / size + x * (1.0 - 1.0 / size); }
float texelToUnit(float u, float size) { return (u - 0.5 / size) / (1.0 - 1.0 / size); }

vec2 transmittanceUv(float r, float mu)
{
    float bottom = bottomRadius(), top = topRadius();
    float H = sqrt(top * top - bottom * bottom);
    float h = max(r - bottom, 0.0);
    float rho = skySafeSqrt(h * (2.0 * bottom + h));
    float d = distanceToTop(r, mu);
    float dMin = top - r;
    float dMax = rho + H;
    float xMu = (d - dMin) / max(dMax - dMin, 1.0e-6);
    float xR = rho / H;
    return vec2(unitToTexel(clamp(xMu, 0.0, 1.0), kTransmittanceLutSize.x),
                unitToTexel(clamp(xR, 0.0, 1.0), kTransmittanceLutSize.y));
}

vec3 transmittanceToTop(float r, float mu)
{
    return texture(u_TransmittanceLUT, transmittanceUv(r, mu)).rgb;
}

// Bilinear in log space: with the sun below the horizon Psi_ms falls by orders of magnitude between neighbouring texels
// (3.7 degrees of sun elevation apart), where linear filtering would put kinks into the twilight sky's brightness (A6.4
// time sweep). In daylight the two agree within the LUT's own accuracy.
vec3 multipleScattering(float r, float sunZenithCos)
{
    vec2 uv = clamp(vec2(sunZenithCos * 0.5 + 0.5, (r - bottomRadius()) / (topRadius() - bottomRadius())), 0.0, 1.0);
    uv = vec2(skyFromUnitToSubUvs(uv.x, kMultiScatteringLutSize), skyFromUnitToSubUvs(uv.y, kMultiScatteringLutSize));
    vec2 texel = uv * kMultiScatteringLutSize - 0.5;
    vec2 base = floor(texel);
    vec2 f = texel - base;
    ivec2 i0 = clamp(ivec2(base), ivec2(0), ivec2(int(kMultiScatteringLutSize) - 1));
    ivec2 i1 = clamp(ivec2(base) + 1, ivec2(0), ivec2(int(kMultiScatteringLutSize) - 1));
    const float kFloor = 1.0e-30;
    vec3 a = log(max(texelFetch(u_MultiScatteringLUT, ivec2(i0.x, i0.y), 0).rgb, vec3(kFloor)));
    vec3 b = log(max(texelFetch(u_MultiScatteringLUT, ivec2(i1.x, i0.y), 0).rgb, vec3(kFloor)));
    vec3 c = log(max(texelFetch(u_MultiScatteringLUT, ivec2(i0.x, i1.y), 0).rgb, vec3(kFloor)));
    vec3 d = log(max(texelFetch(u_MultiScatteringLUT, ivec2(i1.x, i1.y), 0).rgb, vec3(kFloor)));
    return exp(mix(mix(a, b, f.x), mix(c, d, f.x), f.y));
}

float rayleighPhase(float cosTheta)
{
    return 3.0 / (16.0 * kSkyPi) * (1.0 + cosTheta * cosTheta);
}

// Cornette-Shanks (Hillaire 2020); cosTheta between the view direction and the direction towards the sun.
float miePhase(float g, float cosTheta)
{
    float k = 3.0 / (8.0 * kSkyPi) * (1.0 - g * g) / (2.0 + g * g);
    return k * (1.0 + cosTheta * cosTheta) / pow(max(1.0 + g * g - 2.0 * g * cosTheta, 1.0e-6), 1.5);
}

struct ScatteringResult {
    vec3 luminance;
    vec3 multiScatteringAs1;
};

// Hillaire 2020, IntegrateScatteredLuminance: single scattering of the sun (Rayleigh + Mie phase, or the isotropic
// phase for the multiple-scattering pass) plus, optionally, the multiple-scattering term, along the ray (r0, mu0) up to
// the top of the atmosphere or the ground, with energy-conserving integration per segment; the lit ground (Lambert,
// u_GroundAlbedo) is added where the ray ends on it. sunZenithCos0: the sun's zenith cosine at the ray origin;
// nu: cosine between the ray and the sun. quadratic: segment ends at tMax (i / N)^2 (dense near the observer).
ScatteringResult integrateScattering(float r0, float mu0, float sunZenithCos0, float nu, int steps, bool quadratic,
                                     bool isotropicPhase, bool withMultipleScattering)
{
    ScatteringResult result;
    result.luminance = vec3(0.0);
    result.multiScatteringAs1 = vec3(0.0);
    bool ground = rayHitsGround(r0, mu0);
    float tMax = ground ? distanceToBottom(r0, mu0) : distanceToTop(r0, mu0);
    if (tMax <= 0.0) {
        return result;
    }
    float g = u_MieExtinction.a;
    float phaseR = rayleighPhase(nu);
    float phaseM = miePhase(g, nu);
    vec3 throughput = vec3(1.0);
    float tPrevious = 0.0;
    for (int i = 0; i < steps; ++i) {
        float a = float(i + 1) / float(steps);
        float tNext = quadratic ? tMax * a * a : tMax * a;
        float dt = tNext - tPrevious;
        float t = tPrevious + 0.5 * dt;
        tPrevious = tNext;
        float r;
        float h = heightAlong(r0, mu0, t, r);
        r = bottomRadius() + h;
        float sunZenithCos = clamp((r0 * sunZenithCos0 + t * nu) / r, -1.0, 1.0);
        Medium medium = sampleMedium(h);
        vec3 sampleTransmittance = exp(-medium.extinction * dt);
        // The planet hides the sun below this point's horizon.
        float horizonCos = -skySafeSqrt(h * (2.0 * bottomRadius() + h)) / r;
        float sunVisible = sunZenithCos >= horizonCos ? 1.0 : 0.0;
        vec3 sunTransmittance = transmittanceToTop(r, sunZenithCos) * sunVisible;
        vec3 phaseScattering = isotropicPhase ? medium.scattering * (1.0 / (4.0 * kSkyPi))
                                              : medium.rayleigh * phaseR + medium.mie * phaseM;
        vec3 source = sunTransmittance * phaseScattering;
        if (withMultipleScattering) {
            source += multipleScattering(r, sunZenithCos) * medium.scattering;
        }
        vec3 extinction = max(medium.extinction, vec3(1.0e-9));
        result.luminance += throughput * (source - source * sampleTransmittance) / extinction;
        result.multiScatteringAs1 += throughput * (medium.scattering - medium.scattering * sampleTransmittance) / extinction;
        throughput *= sampleTransmittance;
    }
    if (ground) {
        float r = bottomRadius();
        float sunZenithCos = clamp((r0 * sunZenithCos0 + tMax * nu) / r, -1.0, 1.0);
        vec3 sunTransmittance = transmittanceToTop(r, sunZenithCos);
        result.luminance += throughput * sunTransmittance * clamp(sunZenithCos, 0.0, 1.0) * u_GroundAlbedo.rgb / kSkyPi;
    }
    return result;
}

#ifdef PASS_TRANSMITTANCE
void main()
{
    vec2 uv = gl_FragCoord.xy / kTransmittanceLutSize;
    float bottom = bottomRadius(), top = topRadius();
    float H = sqrt(top * top - bottom * bottom);
    float rho = H * texelToUnit(uv.y, kTransmittanceLutSize.y);
    float r = sqrt(rho * rho + bottom * bottom);
    float dMin = top - r;
    float dMax = rho + H;
    float d = dMin + texelToUnit(uv.x, kTransmittanceLutSize.x) * (dMax - dMin);
    float mu = d <= 0.0 ? 1.0 : clamp((H * H - rho * rho - d * d) / (2.0 * r * d), -1.0, 1.0);
    const int kSteps = 128;
    float rayLength = distanceToTop(r, mu);
    float dt = rayLength / float(kSteps);
    vec3 opticalDepth = vec3(0.0);
    for (int i = 0; i < kSteps; ++i) {
        float rr;
        float h = heightAlong(r, mu, (float(i) + 0.5) * dt, rr);
        opticalDepth += sampleMedium(h).extinction * dt;
    }
    fragmentColor = vec4(exp(-opticalDepth), 1.0);
}
#endif

#ifdef PASS_MULTISCATTER
void main()
{
    vec2 uv = gl_FragCoord.xy / vec2(kMultiScatteringLutSize);
    uv = vec2(skyFromSubUvsToUnit(uv.x, kMultiScatteringLutSize), skyFromSubUvsToUnit(uv.y, kMultiScatteringLutSize));
    uv = clamp(uv, 0.0, 1.0);
    float sunZenithCos = uv.x * 2.0 - 1.0;
    float r = bottomRadius() + max(uv.y * (topRadius() - bottomRadius()), 1.0e-3);
    vec3 sun = vec3(skySafeSqrt(1.0 - sunZenithCos * sunZenithCos), sunZenithCos, 0.0);
    const int kSqrtSamples = 8;
    vec3 luminance = vec3(0.0);
    vec3 multiScatteringAs1 = vec3(0.0);
    for (int i = 0; i < kSqrtSamples; ++i) {
        for (int j = 0; j < kSqrtSamples; ++j) {
            float theta = 2.0 * kSkyPi * (float(i) + 0.5) / float(kSqrtSamples);
            float phi = acos(clamp(1.0 - 2.0 * (float(j) + 0.5) / float(kSqrtSamples), -1.0, 1.0));
            vec3 direction = vec3(cos(theta) * sin(phi), cos(phi), sin(theta) * sin(phi));
            ScatteringResult s = integrateScattering(r, direction.y, sunZenithCos, dot(direction, sun), 32, false, true, false);
            luminance += s.luminance;
            multiScatteringAs1 += s.multiScatteringAs1;
        }
    }
    // Uniform sphere samples: (4 pi / N) x isotropic phase 1 / (4 pi) = 1 / N.
    float invCount = 1.0 / float(kSqrtSamples * kSqrtSamples);
    vec3 secondOrder = luminance * invCount;          // L_2ndorder (Hillaire eq. 5)
    vec3 transfer = multiScatteringAs1 * invCount;     // f_ms (eq. 7)
    fragmentColor = vec4(secondOrder / max(vec3(1.0) - transfer, vec3(1.0e-4)), 1.0); // Psi_ms (eq. 10)
}
#endif

#ifdef PASS_SKYVIEW
void main()
{
    vec2 uv = gl_FragCoord.xy / kSkyViewLutSize;
    float viewRadius = u_AtmosphereRadii.z;
    float viewZenithCos, lightViewCos;
    skyViewLutParams(uv, bottomRadius(), viewRadius, viewZenithCos, lightViewCos);
    float sunZenithCos = clamp(u_SunDirection.y, -1.0, 1.0);
    float sunSin = skySafeSqrt(1.0 - sunZenithCos * sunZenithCos);
    float viewSin = skySafeSqrt(1.0 - viewZenithCos * viewZenithCos);
    float nu = clamp(viewSin * sunSin * lightViewCos + viewZenithCos * sunZenithCos, -1.0, 1.0);
    ScatteringResult s = integrateScattering(viewRadius, viewZenithCos, sunZenithCos, nu, 48, true, false, true);
    // A6.6 night sky glow (airglow and the integrated starlight; NightSky.h): a thin emitting layer at u_Airglow.a km,
    // brighter towards the horizon by the van Rhijn factor 1 / sqrt(1 - (R / (R + h))^2 sin^2 z), dimmed by the
    // luminance of the transmittance to the top of the atmosphere. Rays that end on the ground see it lit by the glow's irradiance
    // (about 1.6 pi x the zenith radiance, NightSky.h airglowIlluminance) through the Lambert ground albedo.
    vec3 glow = vec3(0.0);
    if (rayHitsGround(viewRadius, viewZenithCos)) {
        glow = u_Airglow.rgb * 1.6 * u_GroundAlbedo.rgb;
    } else {
        float ratio = bottomRadius() / (bottomRadius() + u_Airglow.a);
        float sinZenith2 = max(1.0 - viewZenithCos * viewZenithCos, 0.0);
        float vanRhijn = inversesqrt(max(1.0 - ratio * ratio * sinZenith2, 1.0e-4));
        // Achromatic extinction: the glow scattered back into the line of sight from the whole sky dome largely refills
        // the colour the direct path loses near the horizon (not modelled per wavelength).
        vec3 transmittance = transmittanceToTop(viewRadius, viewZenithCos);
        glow = u_Airglow.rgb * vanRhijn * dot(transmittance, vec3(0.2126, 0.7152, 0.0722));
    }
    fragmentColor = vec4(s.luminance + glow, 1.0);
}
#endif

#ifdef PASS_CUBE
// The face orientation of ibl_panorama_to_cubemap.frag (its table is written with t pointing up; the cube faces store
// t pointing down, hence the flipped y).
vec3 cubeDirection(int face, vec2 uv)
{
    if (face == 0) return vec3(1.0, uv.y, -uv.x);
    if (face == 1) return vec3(-1.0, uv.y, uv.x);
    if (face == 2) return vec3(uv.x, -1.0, uv.y);
    if (face == 3) return vec3(uv.x, 1.0, -uv.y);
    if (face == 4) return vec3(uv.x, uv.y, 1.0);
    return vec3(-uv.x, uv.y, -1.0);
}

void main()
{
    vec2 uv = gl_FragCoord.xy / u_TargetSize * 2.0 - 1.0;
    vec3 direction = normalize(cubeDirection(u_Face, uv));
    direction.y = -direction.y;
    vec3 radiance = skyViewRadiance(u_SkyViewLUT, direction, u_SunDirection, bottomRadius(), u_AtmosphereRadii.z);
    fragmentColor = vec4(radiance * u_RadianceScale, 1.0);
}
#endif
