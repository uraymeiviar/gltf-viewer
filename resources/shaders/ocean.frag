// A6.8: the ocean surface's shading (docs/ASOBO_EXTENSIONS_PHASE_6.md, A6.8), in linear HDR like the A3.6 plane.
//  * Normal: per cascade, central differences of the displaced surface (x + Dx, h, z + Dz) at the cascade's mip level
//    for this pixel's footprint (the two frames around the current time interpolated). The mean square slope that the
//    mip levels and the finest cascade leave out (OceanSimulation's tables; Cox and Munk 1954 for the total) is added
//    to the GGX roughness (alpha^2 = sum of the slope variances), so the far sea becomes a rougher averaged surface
//    instead of aliasing sparkle.
//  * Fresnel (water IOR 1.33, F0 = 0.02) between the reflected environment (the GGX cube of the active source; a
//    reflection pointing below the horizon is mirrored above it) and the light the water body scatters back
//    (u_OceanColor x the sky's diffuse light and the sun, plus a forward-scattering crest term towards the sun); the
//    A3.1 sun's GGX highlight with the A3.2 shadow visibility; foam (a white Lambert layer) where the accumulated
//    Jacobian is below the threshold; the A3.8 ambient light; the plane's horizon fog.
//  * Model interiors: with the A3.9 sky-visibility volume, water where less than 5% of the sky is visible (inside
//    closed hulls and cabins, inside solids) is discarded; without it, while the camera is inside the model's bounds,
//    the water inside those bounds is discarded.
// Writes the fragment's depth (1 beyond the far plane, like the plane), so hulls and floats intersect the waves.
#include <camera_block.glsl>
#include <environment_block.glsl>

#include <ground_common.glsl>
#include <brdf.glsl>
#include <ibl.glsl>
#include <shadow.glsl>
#include <sky_occlusion.glsl>

in vec3 v_WorldPosition;
in vec2 v_SurfaceCoord;
in float v_Height;
out vec4 g_finalColor;

uniform sampler2DArray u_OceanDisplacement;
uniform vec4 u_OceanGrid;
uniform vec4 u_OceanInfo;
uniform vec4 u_OceanCascade0;
uniform vec4 u_OceanCascade1;
uniform vec4 u_OceanCascade2;
uniform vec4 u_OceanCascade3;
// Per cascade: the mean square slope lost at mip levels 0..3 and 4..7 (level 7: the whole band).
uniform vec4 u_OceanSlopeA0;
uniform vec4 u_OceanSlopeB0;
uniform vec4 u_OceanSlopeA1;
uniform vec4 u_OceanSlopeB1;
uniform vec4 u_OceanSlopeA2;
uniform vec4 u_OceanSlopeB2;
uniform vec4 u_OceanSlopeA3;
uniform vec4 u_OceanSlopeB3;
uniform vec3 u_OceanColor;          // linear, the light scattered back out of the water body
uniform vec4 u_OceanParams;         // x: significant wave height (m)
uniform vec4 u_OceanFoam;           // x: z of the whitecap coverage (P(Z > z) = coverage), y: sigma of J at full resolution, z: 1 / total slope variance, w: edge width / sigma
uniform vec4 u_OceanInteriorMin;    // xyz: model bounds minimum, w: 1 = the camera is inside the bounds
uniform vec4 u_OceanInteriorMax;    // xyz: model bounds maximum
uniform vec2 u_GroundHaze;          // as ground.frag
uniform vec3 u_GroundHazeColor;

const float OCEAN_F0 = 0.02;   // ((1.33 - 1) / (1.33 + 1))^2
const float FOAM_ALBEDO = 0.8;
// A shadow removes the sun's direct specular and the sun-driven part of the water body's scattering, but the light that
// entered the water outside the shadow diffuses sideways (diffusion lengths of metres to tens of metres, more than the
// width of a hull's shadow): a third of the sun-driven body light remains. The sky reflection and the sky-lit body
// light are never touched by the shadow.
const float OCEAN_SHADOW_FILL = 0.3;
// The body is lighter at the wave crests and darker in the troughs (thinner / thicker water layer, more light scattered
// towards the viewer): +-35% at the significant wave height.
const float OCEAN_CREST_TINT = 0.35;
const float FOAM_MEAN = -0.03;

float oceanHash(vec2 p)
{
    vec3 q = fract(vec3(p.xyx) * 0.1031);
    q += dot(q, q.yzx + 33.33);
    return fract((q.x + q.y) * q.z);
}

float oceanValueNoise(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(oceanHash(i), oceanHash(i + vec2(1.0, 0.0)), f.x),
               mix(oceanHash(i + vec2(0.0, 1.0)), oceanHash(i + vec2(1.0, 1.0)), f.x), f.y);
}

float oceanSlopeLookup(vec4 a, vec4 b, float lod)
{
    float l = clamp(lod, 0.0, 7.0);
    int i = int(floor(l));
    int j = min(i + 1, 7);
    float vi = i < 4 ? a[i] : b[i - 4];
    float vj = j < 4 ? a[j] : b[j - 4];
    return mix(vi, vj, l - float(i));
}

vec4 oceanSample(vec4 cascade, vec2 uv, float lod)
{
    vec4 a = textureLod(u_OceanDisplacement, vec3(uv, cascade.y), lod);
    vec4 b = textureLod(u_OceanDisplacement, vec3(uv, cascade.z), lod);
    return mix(a, b, cascade.w);
}

// Adds a cascade's surface derivatives (d/dx and d/dz of (Dx, h, Dz)), its part of the summed Jacobian (J - 1, the
// compression of its displacement, persisting for a while after a fold) and its unresolved slope variance at this
// pixel's footprint.
void oceanCascade(vec4 cascade, vec4 slopeA, vec4 slopeB, vec2 xz, float footprint,
                  inout vec3 dPdx, inout vec3 dPdz, inout float jacobian, inout float foamSlope, inout float slopeVariance)
{
    float n = u_OceanInfo.y;
    float texel = 1.0 / (cascade.x * n);
    float lod = max(log2(footprint / texel), 0.0);
    float stepSize = texel * exp2(lod);
    vec2 uv = xz * cascade.x;
    float du = stepSize * cascade.x;
    vec4 px = oceanSample(cascade, uv + vec2(du, 0.0), lod);
    vec4 mx = oceanSample(cascade, uv - vec2(du, 0.0), lod);
    vec4 pz = oceanSample(cascade, uv + vec2(0.0, du), lod);
    vec4 mz = oceanSample(cascade, uv - vec2(0.0, du), lod);
    float inv = 0.5 / stepSize;
    dPdx += (px.xyz - mx.xyz) * inv;
    dPdz += (pz.xyz - mz.xyz) * inv;
    jacobian += oceanSample(cascade, uv, lod).w - 1.0;
    float lost = oceanSlopeLookup(slopeA, slopeB, lod);
    slopeVariance += lost;
    foamSlope += slopeB.w - lost; // what this cascade still resolves at this footprint (its J varies with it)
}

void main()
{
    vec3 position = v_WorldPosition;

#ifdef SKY_OCCLUSION
    if (skyOcclusionMoments(position, vec3(0.0, 1.0, 0.0)).x < 0.05) {
        discard;
    }
#else
    if (u_OceanInteriorMin.w > 0.5 && all(greaterThan(position, u_OceanInteriorMin.xyz)) &&
        all(lessThan(position, u_OceanInteriorMax.xyz))) {
        discard;
    }
#endif

    vec4 clip = u_ViewProjectionMatrix * vec4(position, 1.0);
    gl_FragDepth = clamp(clip.z / clip.w * 0.5 + 0.5, 0.0, 1.0);

    vec2 xz = v_SurfaceCoord;
    float footprint = max(max(length(dFdx(xz)), length(dFdy(xz))), 1.0e-5);
    vec3 dPdx = vec3(1.0, 0.0, 0.0);
    vec3 dPdz = vec3(0.0, 0.0, 1.0);
    float jacobian = 1.0; // the Jacobian of the summed displacement: 1 + the sum of the cascades' (J - 1)
    float foamSlope = 0.0;
    float slopeVariance = u_OceanInfo.w;
    int count = int(u_OceanInfo.x + 0.5);
    if (count > 0) oceanCascade(u_OceanCascade0, u_OceanSlopeA0, u_OceanSlopeB0, xz, footprint, dPdx, dPdz, jacobian, foamSlope, slopeVariance);
    if (count > 1) oceanCascade(u_OceanCascade1, u_OceanSlopeA1, u_OceanSlopeB1, xz, footprint, dPdx, dPdz, jacobian, foamSlope, slopeVariance);
    if (count > 2) oceanCascade(u_OceanCascade2, u_OceanSlopeA2, u_OceanSlopeB2, xz, footprint, dPdx, dPdz, jacobian, foamSlope, slopeVariance);
    if (count > 3) oceanCascade(u_OceanCascade3, u_OceanSlopeA3, u_OceanSlopeB3, xz, footprint, dPdx, dPdz, jacobian, foamSlope, slopeVariance);
    // Whitecaps: the summed Jacobian J (1 + the sum of the cascades' J - 1) is about Gaussian, FOAM_MEAN +- sigma; sigma is
    // u_OceanFoam.y at full resolution and narrows with the slope variance the footprint still resolves (distance). The
    // threshold z sigma below the mean covers the whitecap fraction of the sea whatever the distance. The fold
    // statistics of the stored (central-difference, persisted) Jacobian were measured, not derived (OceanSimulation.h).
    float sigma = u_OceanFoam.y * sqrt(clamp(foamSlope * u_OceanFoam.z, 0.0, 1.0));
    // Lace: the threshold is perturbed by noise fixed to the undisplaced water coordinate (foam moves with the water, not
    // with the crests), fading out once a pixel covers more than a few of its cells.
    float lace = (0.65 * oceanValueNoise(xz * 2.3) + 0.35 * oceanValueNoise(xz * 8.7 + 17.0)) - 0.5;
    lace *= 1.0 - smoothstep(0.04, 0.3, footprint);
    float foam = clamp((1.0 + FOAM_MEAN - u_OceanFoam.x * sigma + 1.6 * sigma * lace - jacobian) / max(u_OceanFoam.w * sigma, 0.02), 0.0, 1.0);

    vec3 n = normalize(cross(dPdz, dPdx));
    vec3 toCamera = u_Camera - position;
    float distanceToCamera = length(toCamera);
    vec3 v = toCamera / max(distanceToCamera, 1.0e-6);
    // Seen from below the sea level: the same surface, lit as if facing the camera.
    if (u_Camera.y < u_OceanGrid.x) {
        n = -n;
    }

    float alphaRoughness = sqrt(clamp(4.0e-4 + slopeVariance, 4.0e-4, 1.0));
    float perceptualRoughness = sqrt(alphaRoughness);
    float NdotV = max(dot(n, v), 1.0e-3);

    vec3 hazeDirection = normalize(vec3(-v.x, 0.0, -v.z) + vec3(0.0, 0.004, 0.0));
    vec3 hazeColor = u_GroundHazeColor;

    // Directional albedo of the water surface for the environment (the split sum of getIBLRadianceGGX with F0 = 0.02).
    float specularAlbedo = OCEAN_F0;
    vec3 reflected = vec3(0.0);
    vec3 skyDiffuse = vec3(0.0);
    vec3 foamDiffuse = vec3(0.0);
#ifdef GROUND_ENV
    if (u_GroundHaze.y > 0.5) {
        float hazeLod = u_EnvBlurNormalized * float(u_MipCount - 1) + 1.0;
        hazeColor = getSpecularSample(hazeDirection, hazeLod).rgb;
    }
    vec2 ab = texture(u_GGXLUT, clamp(vec2(NdotV, perceptualRoughness), vec2(0.0), vec2(1.0))).rg;
    float fr = max(1.0 - perceptualRoughness, OCEAN_F0) - OCEAN_F0;
    specularAlbedo = (OCEAN_F0 + fr * pow(1.0 - NdotV, 5.0)) * ab.x + ab.y;
#ifdef USE_IBL
    vec3 r = reflect(-v, n);
    r.y = abs(r.y); // a reflection towards the sea would see more water: take the sky above it
    reflected = getSpecularSample(normalize(r), perceptualRoughness * float(u_MipCount - 1)).rgb * specularAlbedo;
    skyDiffuse = getDiffuseLight(vec3(0.0, 1.0, 0.0));
    foamDiffuse = getDiffuseLight(n);
#endif
#endif
    if (u_AmbientEnabled != 0 && (u_AmbientEnabled == 1 || u_SkyOcclusionParams.y == 0.0)) {
        reflected += u_AmbientRadiance * specularAlbedo;
        skyDiffuse += u_AmbientRadiance;
        foamDiffuse += u_AmbientRadiance;
    }
    vec3 body = u_OceanColor * skyDiffuse * (1.0 - specularAlbedo);
    vec3 sunSpecular = vec3(0.0);
    if (u_SunEnabled != 0) {
        float visibility = sunShadowVisibility(position, vec3(0.0, 1.0, 0.0));
        vec3 sunIntensity = u_SunIlluminance * visibility;
        vec3 sunBody = u_SunIlluminance * mix(OCEAN_SHADOW_FILL, 1.0, visibility);
        vec3 l = normalize(u_SunDirection);
        vec3 h = normalize(l + v);
        float NdotL = clampedDot(n, l);
        float NdotH = clampedDot(n, h);
        float VdotH = clampedDot(v, h);
        float sunAlpha = min(alphaRoughness + 0.5 * tan(u_SunAngularRadius), 1.0);
        if (NdotL > 0.0) {
            sunSpecular = sunIntensity * NdotL *
                          BRDF_specularGGX(vec3(OCEAN_F0), vec3(1.0), sunAlpha, 1.0, VdotH, NdotL, NdotV, NdotH);
        }
        float sunUp = max(l.y, 0.0);
        body += u_OceanColor * sunBody * (sunUp / M_PI) * (1.0 - OCEAN_F0);
        // Light passing through the wave crests towards a viewer who looks towards the sun.
        float crest = clamp(v_Height / max(0.5 * u_OceanParams.x, 0.05), 0.0, 1.0);
        float forward = pow(clampedDot(l, -v), 4.0) * pow(clamp(0.5 - 0.5 * dot(l, n), 0.0, 1.0), 3.0);
        body += u_OceanColor * sunIntensity * (4.0 * crest * forward * sunUp / M_PI);
        foamDiffuse += sunIntensity * NdotL / M_PI;
    }

    float level = clamp(v_Height / max(u_OceanParams.x, 0.1), -1.0, 1.0);
    body *= 1.0 + OCEAN_CREST_TINT * level;
    vec3 color = reflected + body + sunSpecular;
    color = mix(color, FOAM_ALBEDO * foamDiffuse, foam);

    float fog = 1.0 - exp(-u_GroundHaze.x * distanceToCamera);
    color = mix(color, hazeColor, fog);

    if (u_PreExposure != 1.0) color *= u_PreExposure;
    g_finalColor = vec4(fitHalfFloatHdr(max(color, vec3(0.0))), 1.0);
}
