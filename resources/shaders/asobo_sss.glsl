// A3.4: ASOBO_material_SSS (docs/ASOBO_EXTENSIONS_SEMANTICS.md section 18). A cheap, stable
// subsurface-scattering approximation for opaque SSS (and Hair) materials, with no screen-space pass:
//   * wrapped diffuse for direct lights, the wrap amount per channel scaled by SSSColor, and
//   * back-lit translucency: the diffuse light arriving from the opposite side (-N), tinted by
//     SSSColor x the diffuse colour and scaled by the transmission weight of opacityTexture.
// The SDK describes SSSColor only as the colour "blended with the incoming light after it has been
// scattered"; the formulas and the two constants below are inferred. Black SSSColor adds exactly zero,
// so such a material renders like the core material. createScene defines ASOBO_SSS per viewport when the
// viewport applies the extension; off compiles the core variant.
#ifdef ASOBO_SSS

uniform vec3 u_AsoboSSSColor; // linear RGB, clamped to 0..1; the exporters' default is white

#ifdef HAS_ASOBO_SSS_OPACITY_MAP
uniform sampler2D u_AsoboSssOpacitySampler;
uniform int u_AsoboSssOpacityUVSet;
#endif

// Largest wrap (at SSSColor 1): the terminator moves to N.L = -0.5.
const float kAsoboSssWrap = 0.5;
// Fraction of the light arriving at the back that is transmitted (at SSSColor 1, transmission 1).
const float kAsoboSssTranslucency = 0.5;

// Transmission weight 0..1 at this pixel: 1 - opacityTexture.r (opacity 1 = opaque, no light through),
// 1 without the texture.
float getAsoboSssTransmission()
{
#ifdef HAS_ASOBO_SSS_OPACITY_MAP
    vec2 uv = u_AsoboSssOpacityUVSet < 1 ? getAsoboMainUV(v_texcoord_0) : v_texcoord_1;
    float opacity = texture(u_AsoboSssOpacitySampler, clampAsoboUV(uv, u_AsoboSssOpacityUVSet, u_AsoboSssOpacitySampler)).r;
    return 1.0 - clamp(opacity, 0.0, 1.0);
#else
    return 1.0;
#endif
}

// Diffuse response of one direct light (rawNdotL = dot(n, l), unclamped), as two per-channel factors of
// the light's irradiance:
//   front = clamp((N.L + w) / (1 + w), 0, 1), w = kAsoboSssWrap * sssColor   (replaces the core clamp(N.L))
//   back  = kAsoboSssTranslucency * sssColor * transmission * clamp(-N.L, 0, 1)
// front + back <= 1 for every N.L (front <= w / (1 + w) <= 1/3 where back > 0), so a surface never
// receives more than a surface facing the light. sssColor = 0 gives front = clamp(N.L, 0, 1), back = 0.
vec3 asoboSssWrapFactor(float rawNdotL, vec3 sssColor)
{
    vec3 w = kAsoboSssWrap * sssColor;
    return clamp((vec3(rawNdotL) + w) / (vec3(1.0) + w), 0.0, 1.0);
}

vec3 asoboSssBackFactor(float rawNdotL, vec3 sssColor, float transmission)
{
    return kAsoboSssTranslucency * sssColor * transmission * clamp(-rawNdotL, 0.0, 1.0);
}

// What SSS adds to the core diffuse of one direct light of irradiance `intensity` (the core adds
// intensity * NdotL * BRDF_lambertian(VdotH)). Any direct light can call it: the punctual lights now, the
// A3.1 sun later. The wrapped part keeps the core Lambert term (with its half-vector Fresnel); the
// transmitted part leaves through the front surface towards the viewer, so its Fresnel weight uses N.V
// (the half vector of a light behind the surface is meaningless and can be the zero vector).
vec3 asoboSssDirectDiffuse(vec3 intensity, float rawNdotL, float NdotL, float VdotH, float NdotV,
                           vec3 f0, vec3 f90, vec3 diffuseColor, float specularWeight, vec3 sssColor, float transmission)
{
    vec3 wrapped = (asoboSssWrapFactor(rawNdotL, sssColor) - vec3(NdotL))
        * BRDF_lambertian(f0, f90, diffuseColor, specularWeight, VdotH);
    vec3 transmitted = asoboSssBackFactor(rawNdotL, sssColor, transmission)
        * BRDF_lambertian(f0, f90, diffuseColor, specularWeight, NdotV);
    return intensity * (wrapped + transmitted);
}

// Only the transmitted part of asoboSssDirectDiffuse (A3.2: the sun shadows it with the visibility of the
// surface's back side, separately from the wrapped front part).
vec3 asoboSssTransmittedDiffuse(vec3 intensity, float rawNdotL, float NdotV, vec3 f0, vec3 f90, vec3 diffuseColor,
                                float specularWeight, vec3 sssColor, float transmission)
{
    return intensity * asoboSssBackFactor(rawNdotL, sssColor, transmission)
        * BRDF_lambertian(f0, f90, diffuseColor, specularWeight, NdotV);
}

// IBL translucency: the diffuse irradiance from the opposite side (backIrradiance = getDiffuseLight(-n)),
// transmitted with the weight of a direct light from straight behind and the same N.V Fresnel. The IBL
// diffuse convention has no 1/pi (diffuse = c_diff * irradiance).
vec3 asoboSssBackRadiance(vec3 backIrradiance, float NdotV, vec3 f0, vec3 f90, vec3 diffuseColor,
                          float specularWeight, vec3 sssColor, float transmission)
{
    vec3 fresnel = vec3(1.0) - specularWeight * F_Schlick(f0, f90, NdotV);
    return kAsoboSssTranslucency * sssColor * transmission * fresnel * diffuseColor * backIrradiance;
}

#endif
