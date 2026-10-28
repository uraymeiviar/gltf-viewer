// A6.2 procedural sky: the sky-view LUT lookup shared by the background (cubemap.frag with PROCEDURAL_SKY) and the
// sky passes (sky_passes.frag). Model: Hillaire 2020, "A Scalable and Production Ready Sky and Atmosphere Rendering
// Technique" (transmittance, multiple-scattering and sky-view LUTs) with Bruneton 2017's Earth atmosphere;
// conventions in header/imgui/SkyAtmosphere.h and docs/ASOBO_EXTENSIONS_PHASE_6.md (A6.2).
//
// The planet is centred at the origin, the observer at (0, viewRadius, 0): world +Y (glTF up) is the local zenith,
// distances in km. The sky-view LUT (192 x 108) stores the radiance seen from the observer per unit top-of-atmosphere
// sun illuminance: u = sqrt of the relative azimuth to the sun (0 towards the sun, 1 away from it), v = the view zenith
// angle, non-linear around the horizon (v < 0.5 above the geometric horizon, v >= 0.5 towards the ground).

// cubemap.frag's includes are prepended whatever its defines say, so the HDR background compiles none of this.
#if defined(PROCEDURAL_SKY) || defined(SKY_PASSES)

const float kSkyPi = 3.14159265358979323846;
const vec2 kSkyViewLutSize = vec2(192.0, 108.0);

float skySafeSqrt(float x)
{
    return sqrt(max(x, 0.0));
}

// Hillaire's sub-texel remapping: the centres of the first and last texel map to 0 and 1.
float skyFromUnitToSubUvs(float u, float resolution)
{
    return (u + 0.5 / resolution) * (resolution / (resolution + 1.0));
}

float skyFromSubUvsToUnit(float u, float resolution)
{
    return (u - 0.5 / resolution) * (resolution / (resolution - 1.0));
}

// Angle between the zenith and the geometric horizon seen from viewRadius (pi - beta, beta = asin(bottom / r)).
void skyHorizonAngles(float bottomRadius, float viewRadius, out float zenithHorizonAngle, out float beta)
{
    float height = max(viewRadius - bottomRadius, 0.0);
    float horizonDistance = skySafeSqrt(height * (2.0 * bottomRadius + height)); // sqrt(r^2 - R^2), cancellation free
    beta = acos(clamp(horizonDistance / viewRadius, -1.0, 1.0));
    zenithHorizonAngle = kSkyPi - beta;
}

vec2 skyViewLutUv(float bottomRadius, float viewRadius, float viewZenithCos, float lightViewCos)
{
    float zenithHorizonAngle, beta;
    skyHorizonAngles(bottomRadius, viewRadius, zenithHorizonAngle, beta);
    float viewZenithAngle = acos(clamp(viewZenithCos, -1.0, 1.0));
    vec2 uv;
    if (viewZenithAngle < zenithHorizonAngle) {
        float coord = viewZenithAngle / zenithHorizonAngle;
        coord = 1.0 - skySafeSqrt(1.0 - coord);
        uv.y = coord * 0.5;
    } else {
        float coord = (viewZenithAngle - zenithHorizonAngle) / max(beta, 1.0e-6);
        uv.y = skySafeSqrt(clamp(coord, 0.0, 1.0)) * 0.5 + 0.5;
    }
    uv.x = skySafeSqrt(clamp(-lightViewCos * 0.5 + 0.5, 0.0, 1.0));
    return vec2(skyFromUnitToSubUvs(uv.x, kSkyViewLutSize.x), skyFromUnitToSubUvs(uv.y, kSkyViewLutSize.y));
}

// Inverse of skyViewLutUv for a texel centre.
void skyViewLutParams(vec2 uv, float bottomRadius, float viewRadius, out float viewZenithCos, out float lightViewCos)
{
    uv = vec2(skyFromSubUvsToUnit(uv.x, kSkyViewLutSize.x), skyFromSubUvsToUnit(uv.y, kSkyViewLutSize.y));
    uv = clamp(uv, 0.0, 1.0);
    float zenithHorizonAngle, beta;
    skyHorizonAngles(bottomRadius, viewRadius, zenithHorizonAngle, beta);
    if (uv.y < 0.5) {
        float coord = 1.0 - 2.0 * uv.y;
        coord = 1.0 - coord * coord;
        viewZenithCos = cos(zenithHorizonAngle * coord);
    } else {
        float coord = uv.y * 2.0 - 1.0;
        viewZenithCos = cos(zenithHorizonAngle + beta * coord * coord);
    }
    lightViewCos = -(uv.x * uv.x * 2.0 - 1.0);
}

// Sky radiance (per unit top-of-atmosphere sun illuminance) in the world direction `direction` for the sun direction
// `sunDirection` (both unit, world space, +Y up).
vec3 skyViewRadiance(sampler2D skyViewLut, vec3 direction, vec3 sunDirection, float bottomRadius, float viewRadius)
{
    float viewZenithCos = clamp(direction.y, -1.0, 1.0);
    float viewHorizontal = length(direction.xz);
    float sunHorizontal = length(sunDirection.xz);
    float lightViewCos = 1.0;
    if (viewHorizontal > 1.0e-6 && sunHorizontal > 1.0e-6) {
        lightViewCos = clamp(dot(direction.xz, sunDirection.xz) / (viewHorizontal * sunHorizontal), -1.0, 1.0);
    }
    return texture(skyViewLut, skyViewLutUv(bottomRadius, viewRadius, viewZenithCos, lightViewCos)).rgb;
}

#endif
