// A3.9: sky occlusion inside the model (docs/ASOBO_EXTENSIONS_PHASE_3.md, A3.9; header/imgui/SkyOcclusion.h).
// Compiled only into materials of a viewport that applies it (define SKY_OCCLUSION, set per viewport), so every other
// render is unchanged. u_SkyOcclusionVolume (RGBA16F, nearest): per cell (a, m), a = visible fraction of the sphere,
// m = mean visible direction x a (an L1 projection of the visibility); a = -1 marks a cell inside a solid.
// The volume's placement is in the EnvironmentBlock (u_SkyOcclusionOrigin / Dims / Params).
#ifdef SKY_OCCLUSION
uniform sampler3D u_SkyOcclusionVolume;

// (a, m) at a surface point: the position is moved u_SkyOcclusionDims.w cells along the geometric normal (towards the
// viewer's side), then the eight surrounding cells are interpolated trilinearly, leaving out cells inside solids (their
// weight goes to the others). Outside the volume: open sky (1, 0). All eight inside solids: closed (0, 0).
vec4 skyOcclusionMoments(vec3 worldPosition, vec3 offsetNormal)
{
    float cell = u_SkyOcclusionOrigin.w;
    vec3 dims = u_SkyOcclusionDims.xyz;
    vec3 p = worldPosition + offsetNormal * (u_SkyOcclusionDims.w * cell);
    vec3 g = (p - u_SkyOcclusionOrigin.xyz) / cell - 0.5;
    if (any(lessThan(g, vec3(-0.5))) || any(greaterThan(g, dims - 0.5))) {
        return vec4(1.0, 0.0, 0.0, 0.0);
    }
    vec3 base = floor(g);
    vec3 f = g - base;
    ivec3 i0 = ivec3(base);
    ivec3 maxIndex = ivec3(dims) - 1;
    vec4 sum = vec4(0.0);
    float weightSum = 0.0;
    for (int k = 0; k < 8; ++k) {
        ivec3 o = ivec3(k & 1, (k >> 1) & 1, (k >> 2) & 1);
        vec3 w3 = mix(1.0 - f, f, vec3(o));
        float w = w3.x * w3.y * w3.z;
        vec4 s = texelFetch(u_SkyOcclusionVolume, clamp(i0 + o, ivec3(0), maxIndex), 0);
        if (s.x >= 0.0 && w > 0.0) {
            sum += w * s;
            weightSum += w;
        }
    }
    return weightSum > 1.0e-6 ? sum / weightSum : vec4(0.0);
}

// (1 / pi) x the irradiance from a uniform unit-radiance spherical cap of angular radius sigma (sin^2 sigma given, sigma <=
// 90 deg) whose centre lies at cosTheta from the normal, with the part below the horizon cut off (Lagarde and de
// Rousiers 2014, "Moving Frostbite to PBR", sphere / disk light illuminance, after Snyder 1996).
float skyCapIrradiance(float cosTheta, float sinSigmaSq)
{
    if (cosTheta * cosTheta > sinSigmaSq) {
        return sinSigmaSq * max(cosTheta, 0.0);
    }
    float sinTheta = sqrt(max(1.0 - cosTheta * cosTheta, 0.0));
    float x = sqrt(max(1.0 / max(sinSigmaSq, 1.0e-8) - 1.0, 0.0));
    float y = clamp(-x * cosTheta / max(sinTheta, 1.0e-6), -1.0, 1.0);
    float sinThetaSqrtY = sinTheta * sqrt(max(1.0 - y * y, 0.0));
    return max(((cosTheta * acos(y) - x * sinThetaSqrtY) * sinSigmaSq + atan(sinThetaSqrtY, x)) / 3.141592653589793, 0.0);
}

// Cosine-weighted visibility around n (the fraction of an open half-space's sky irradiance that arrives), scaled so
// u_SkyOcclusionParams.x of an open half-space (the direction lattice's noise floor) reads 1. Two readings of (a, m):
//  * L1: a + 2 m.n, exact for a visibility step through the cell (a half-space) but 25% low for a small opening (the
//    clamped cosine's band 2 is missing);
//  * a visibility cone (axis m, solid angle 4 pi a: cos alpha = 1 - 2a) lit like a spherical cap light (a > 0.5: the
//    open sphere minus the blocked cap around -m), exact for one opening or one blocker of any size.
// They are blended by how cap-like the visible set is: |m| against the |m| of a cap with that solid angle,
// sin^2(alpha) / 4 (1 for one opening, 0 for visibility spread symmetrically around the cell).
float skyDiffuseVisibility(vec4 moments, vec3 n)
{
    float a = clamp(moments.x, 0.0, 1.0);
    float l1 = a + 2.0 * dot(moments.yzw, n);
    float axisLength = length(moments.yzw);
    float cosAlpha = 1.0 - 2.0 * a;
    float sinAlphaSq = max(1.0 - cosAlpha * cosAlpha, 0.0);
    float capLength = 0.25 * sinAlphaSq;
    float visibility = l1;
    if (axisLength > 1.0e-6 && capLength > 1.0e-6) {
        float coherence = clamp(axisLength / capLength, 0.0, 1.0);
        float cosTheta = dot(n, moments.yzw / axisLength);
        float cone = a <= 0.5 ? skyCapIrradiance(cosTheta, sinAlphaSq) : 1.0 - skyCapIrradiance(-cosTheta, sinAlphaSq);
        visibility = mix(l1, cone, coherence);
    }
    return clamp(visibility * u_SkyOcclusionParams.x, 0.0, 1.0);
}

// Solid angle of the intersection of two spherical caps (half-angles r1, r2, axes d apart) divided by 2 pi, with the
// smoothstep approximation of Oat and Sander 2007 ("Ambient aperture lighting").
float skyCapIntersection(float r1, float r2, float d)
{
    float rMin = min(r1, r2);
    float rMax = max(r1, r2);
    float area = 1.0 - cos(rMin);
    if (d <= rMax - rMin) {
        return area;
    }
    if (d >= r1 + r2) {
        return 0.0;
    }
    float x = 1.0 - clamp((d - (rMax - rMin)) / (2.0 * rMin), 0.0, 1.0);
    return area * smoothstep(0.0, 1.0, x);
}

// Visibility of a specular lobe around r: the visible directions as a cone (axis m, solid angle 4 pi a) intersected with
// the lobe's cone (half-angle from the roughness), relative to the lobe's intersection with the open half-space above
// n (the part a surface ever sees), then blended towards the cosine-weighted term with the roughness.
float skySpecularVisibility(vec4 moments, vec3 n, vec3 r, float alphaRoughness, float diffuseVisibility)
{
    const float halfPi = 1.5707963267948966;
    float a = clamp(moments.x * u_SkyOcclusionParams.x, 0.0, 1.0);
    float axisLength = length(moments.yzw);
    vec3 bent = axisLength > 1.0e-5 ? moments.yzw / axisLength : n;
    float visibleCone = acos(clamp(1.0 - 2.0 * a, -1.0, 1.0));
    float lobeCone = clamp(alphaRoughness, 0.03, 1.0) * halfPi;
    float open = skyCapIntersection(halfPi, lobeCone, acos(clamp(dot(n, r), -1.0, 1.0)));
    float seen = skyCapIntersection(visibleCone, lobeCone, acos(clamp(dot(bent, r), -1.0, 1.0)));
    float cone = open > 1.0e-6 ? clamp(seen / open, 0.0, 1.0) : 1.0;
    return mix(cone, diffuseVisibility, clamp(alphaRoughness, 0.0, 1.0));
}
#endif
