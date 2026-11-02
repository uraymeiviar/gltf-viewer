
vec3 getDiffuseLight(vec3 n)
{
    return texture(u_LambertianEnvSampler, u_EnvRotation * n).rgb * u_EnvIntensity;
}


vec4 getSpecularSample(vec3 reflection, float lod)
{
    return textureLod(u_GGXEnvSampler, u_EnvRotation * reflection, lod) * u_EnvIntensity;
}


vec4 getSheenSample(vec3 reflection, float lod)
{
    return textureLod(u_CharlieEnvSampler, u_EnvRotation * reflection, lod) * u_EnvIntensity;
}


vec3 getIBLRadianceGGX(vec3 n, vec3 v, float roughness, vec3 F0, float specularWeight)
{
    float NdotV = clampedDot(n, v);
    float lod = roughness * float(u_MipCount - 1);
    vec3 reflection = normalize(reflect(-v, n));

    vec2 brdfSamplePoint = clamp(vec2(NdotV, roughness), vec2(0.0, 0.0), vec2(1.0, 1.0));
    vec2 f_ab = texture(u_GGXLUT, brdfSamplePoint).rg;
    vec4 specularSample = getSpecularSample(reflection, lod);

    vec3 specularLight = specularSample.rgb;

    // see https://bruop.github.io/ibl/#single_scattering_results at Single Scattering Results
    // Roughness dependent fresnel, from Fdez-Aguera
    vec3 Fr = max(vec3(1.0 - roughness), F0) - F0;
    vec3 k_S = F0 + Fr * pow(1.0 - NdotV, 5.0);
    vec3 FssEss = k_S * f_ab.x + f_ab.y;

    return specularWeight * specularLight * FssEss;
}


#ifdef MATERIAL_ANISOTROPY
// KHR_materials_anisotropy (glTF-Sample-Viewer): the reflection vector comes from a normal bent
// towards the anisotropic "normal" (v without its component along anisotropyDirection), by more for a
// stronger, smoother lobe. anisotropyDirection is the smooth axis (anisotropicB); roughness is its
// perceptual roughness.
// iridescenceFactor 0 leaves the k_S of getIBLRadianceGGX unchanged.
vec3 getIBLRadianceAnisotropy(vec3 n, vec3 v, float roughness, float anisotropy, vec3 anisotropyDirection, vec3 F0,
    vec3 iridescenceFresnel, float iridescenceFactor, float specularWeight)
{
    float NdotV = clampedDot(n, v);

    vec3 anisotropicTangent = cross(anisotropyDirection, v);
    vec3 anisotropicNormal = cross(anisotropicTangent, anisotropyDirection);
    float bendFactor = 1.0 - anisotropy * (1.0 - roughness);
    float bendFactorPow4 = bendFactor * bendFactor * bendFactor * bendFactor;
    vec3 bent = mix(anisotropicNormal, n, bendFactorPow4);
    // v parallel to the direction makes the anisotropic normal vanish; keep n then.
    vec3 bentNormal = dot(bent, bent) > 1e-12 ? normalize(bent) : n;

    float lod = roughness * float(u_MipCount - 1);
    vec3 reflection = normalize(reflect(-v, bentNormal));

    vec2 brdfSamplePoint = clamp(vec2(NdotV, roughness), vec2(0.0, 0.0), vec2(1.0, 1.0));
    vec2 f_ab = texture(u_GGXLUT, brdfSamplePoint).rg;
    vec3 specularLight = getSpecularSample(reflection, lod).rgb;

    vec3 Fr = max(vec3(1.0 - roughness), F0) - F0;
    vec3 k_S = mix(F0 + Fr * pow(1.0 - NdotV, 5.0), iridescenceFresnel, iridescenceFactor);
    vec3 FssEss = k_S * f_ab.x + f_ab.y;

    return specularWeight * specularLight * FssEss;
}
#endif


#ifdef MATERIAL_IRIDESCENCE
vec3 getIBLRadianceGGXIridescence(vec3 n, vec3 v, float roughness, vec3 F0, vec3 iridescenceFresnel, float iridescenceFactor, float specularWeight)
{
    float NdotV = clampedDot(n, v);
    float lod = roughness * float(u_MipCount - 1);
    vec3 reflection = normalize(reflect(-v, n));

    vec2 brdfSamplePoint = clamp(vec2(NdotV, roughness), vec2(0.0, 0.0), vec2(1.0, 1.0));
    vec2 f_ab = texture(u_GGXLUT, brdfSamplePoint).rg;
    vec4 specularSample = getSpecularSample(reflection, lod);

    vec3 specularLight = specularSample.rgb;

    // see https://bruop.github.io/ibl/#single_scattering_results at Single Scattering Results
    // Roughness dependent fresnel, from Fdez-Aguera
    vec3 Fr = max(vec3(1.0 - roughness), F0) - F0;
    vec3 k_S = mix(F0 + Fr * pow(1.0 - NdotV, 5.0), iridescenceFresnel, iridescenceFactor);
    vec3 FssEss = k_S * f_ab.x + f_ab.y;

    return specularWeight * specularLight * FssEss;
}
#endif


#ifdef MATERIAL_TRANSMISSION
vec3 getTransmissionSample(vec2 fragCoord, float roughness, float ior)
{
    float framebufferLod = log2(float(u_TransmissionFramebufferSize.x)) * applyIorToRoughness(roughness, ior);
    vec3 transmittedLight = textureLod(u_TransmissionFramebufferSampler, fragCoord.xy, framebufferLod).rgb;
    if (u_PreExposure != 1.0) transmittedLight /= u_PreExposure; // A6.4: pre-exposed scene colour
    return transmittedLight;
}
#endif


#ifdef MATERIAL_TRANSMISSION
#ifdef MATERIAL_DISPERSION
uniform float u_Dispersion; // KHR_materials_dispersion: 20 / Abbe number
#endif
vec3 getIBLVolumeRefraction(vec3 n, vec3 v, float perceptualRoughness, vec3 baseColor, vec3 f0, vec3 f90,
    vec3 position, mat4 modelMatrix, mat4 viewMatrix, mat4 projMatrix, float ior, float thickness, vec3 attenuationColor, float attenuationDistance)
{
#if defined(MATERIAL_DISPERSION) && defined(MATERIAL_VOLUME)
    // KHR_materials_dispersion (as the Khronos sample viewer): one refraction per colour channel, the IOR spread around
    // the material IOR by halfSpread = (ior - 1) * 0.025 * dispersion (red bends least, blue most). The ray length of
    // the last (blue) channel drives the volume attenuation, like the sample viewer. Dispersion 0 takes the
    // single-sample path below, the same code as a material without the extension.
    vec3 transmittedLight;
    float transmissionRayLength;
    if (u_Dispersion > 0.0) {
        float halfSpread = (ior - 1.0) * 0.025 * u_Dispersion;
        vec3 iors = vec3(ior - halfSpread, ior, ior + halfSpread);
        for (int i = 0; i < 3; i++) {
            vec3 channelRay = getVolumeTransmissionRay(n, v, thickness, iors[i], modelMatrix);
            transmissionRayLength = length(channelRay);
            vec4 channelNdc = projMatrix * viewMatrix * vec4(position + channelRay, 1.0);
            vec2 channelCoords = channelNdc.xy / channelNdc.w;
            channelCoords += 1.0;
            channelCoords /= 2.0;
            transmittedLight[i] = getTransmissionSample(channelCoords, perceptualRoughness, iors[i])[i];
        }
    } else {
        vec3 transmissionRay = getVolumeTransmissionRay(n, v, thickness, ior, modelMatrix);
        vec3 refractedRayExit = position + transmissionRay;

        // Project refracted vector on the framebuffer, while mapping to normalized device coordinates.
        vec4 ndcPos = projMatrix * viewMatrix * vec4(refractedRayExit, 1.0);
        vec2 refractionCoords = ndcPos.xy / ndcPos.w;
        refractionCoords += 1.0;
        refractionCoords /= 2.0;

        // Sample framebuffer to get pixel the refracted ray hits.
        transmittedLight = getTransmissionSample(refractionCoords, perceptualRoughness, ior);
        transmissionRayLength = length(transmissionRay);
    }
    vec3 attenuatedColor = applyVolumeAttenuation(transmittedLight, transmissionRayLength, attenuationColor, attenuationDistance);
#else
    vec3 transmissionRay = getVolumeTransmissionRay(n, v, thickness, ior, modelMatrix);
    vec3 refractedRayExit = position + transmissionRay;

    // Project refracted vector on the framebuffer, while mapping to normalized device coordinates.
    vec4 ndcPos = projMatrix * viewMatrix * vec4(refractedRayExit, 1.0);
    vec2 refractionCoords = ndcPos.xy / ndcPos.w;
    refractionCoords += 1.0;
    refractionCoords /= 2.0;

    // Sample framebuffer to get pixel the refracted ray hits.
    vec3 transmittedLight = getTransmissionSample(refractionCoords, perceptualRoughness, ior);

    vec3 attenuatedColor = applyVolumeAttenuation(transmittedLight, length(transmissionRay), attenuationColor, attenuationDistance);
#endif

    // Sample GGX LUT to get the specular component.
    float NdotV = clampedDot(n, v);
    vec2 brdfSamplePoint = clamp(vec2(NdotV, perceptualRoughness), vec2(0.0, 0.0), vec2(1.0, 1.0));
    vec2 brdf = texture(u_GGXLUT, brdfSamplePoint).rg;
    vec3 specularColor = f0 * brdf.x + f90 * brdf.y;

    return (1.0 - specularColor) * attenuatedColor * baseColor;
}
#endif


// specularWeight is introduced with KHR_materials_specular
vec3 getIBLRadianceLambertian(vec3 n, vec3 v, float roughness, vec3 diffuseColor, vec3 F0, float specularWeight)
{
    float NdotV = clampedDot(n, v);
    vec2 brdfSamplePoint = clamp(vec2(NdotV, roughness), vec2(0.0, 0.0), vec2(1.0, 1.0));
    vec2 f_ab = texture(u_GGXLUT, brdfSamplePoint).rg;

    vec3 irradiance = getDiffuseLight(n);

    // see https://bruop.github.io/ibl/#single_scattering_results at Single Scattering Results
    // Roughness dependent fresnel, from Fdez-Aguera

    vec3 Fr = max(vec3(1.0 - roughness), F0) - F0;
    vec3 k_S = F0 + Fr * pow(1.0 - NdotV, 5.0);
    vec3 FssEss = specularWeight * k_S * f_ab.x + f_ab.y; // <--- GGX / specular light contribution (scale it down if the specularWeight is low)

    // Multiple scattering, from Fdez-Aguera
    float Ems = (1.0 - (f_ab.x + f_ab.y));
    vec3 F_avg = specularWeight * (F0 + (1.0 - F0) / 21.0);
    vec3 FmsEms = Ems * FssEss * F_avg / (1.0 - F_avg * Ems);
    vec3 k_D = diffuseColor * (1.0 - FssEss + FmsEms); // we use +FmsEms as indicated by the formula in the blog post (might be a typo in the implementation)

    return (FmsEms + k_D) * irradiance;
}


#ifdef MATERIAL_IRIDESCENCE
// specularWeight is introduced with KHR_materials_specular
vec3 getIBLRadianceLambertianIridescence(vec3 n, vec3 v, float roughness, vec3 diffuseColor, vec3 F0, vec3 iridescenceF0, float iridescenceFactor, float specularWeight)
{
    float NdotV = clampedDot(n, v);
    vec2 brdfSamplePoint = clamp(vec2(NdotV, roughness), vec2(0.0, 0.0), vec2(1.0, 1.0));
    vec2 f_ab = texture(u_GGXLUT, brdfSamplePoint).rg;

    vec3 irradiance = getDiffuseLight(n);

    // Use the maximum component of the iridescence Fresnel color
    // Maximum is used instead of the RGB value to not get inverse colors for the diffuse BRDF
    vec3 iridescenceF0Max = vec3(max(max(iridescenceF0.r, iridescenceF0.g), iridescenceF0.b));

    // Blend between base F0 and iridescence F0
    vec3 mixedF0 = mix(F0, iridescenceF0Max, iridescenceFactor);

    // see https://bruop.github.io/ibl/#single_scattering_results at Single Scattering Results
    // Roughness dependent fresnel, from Fdez-Aguera

    vec3 Fr = max(vec3(1.0 - roughness), mixedF0) - mixedF0;
    vec3 k_S = mixedF0 + Fr * pow(1.0 - NdotV, 5.0);
    vec3 FssEss = specularWeight * k_S * f_ab.x + f_ab.y; // <--- GGX / specular light contribution (scale it down if the specularWeight is low)

    // Multiple scattering, from Fdez-Aguera
    float Ems = (1.0 - (f_ab.x + f_ab.y));
    vec3 F_avg = specularWeight * (mixedF0 + (1.0 - mixedF0) / 21.0);
    vec3 FmsEms = Ems * FssEss * F_avg / (1.0 - F_avg * Ems);
    vec3 k_D = diffuseColor * (1.0 - FssEss + FmsEms); // we use +FmsEms as indicated by the formula in the blog post (might be a typo in the implementation)

    return (FmsEms + k_D) * irradiance;
}
#endif


vec3 getIBLRadianceCharlie(vec3 n, vec3 v, float sheenRoughness, vec3 sheenColor)
{
    float NdotV = clampedDot(n, v);
    float lod = sheenRoughness * float(u_MipCount - 1);
    vec3 reflection = normalize(reflect(-v, n));

    vec2 brdfSamplePoint = clamp(vec2(NdotV, sheenRoughness), vec2(0.0, 0.0), vec2(1.0, 1.0));
    float brdf = texture(u_CharlieLUT, brdfSamplePoint).b;
    vec4 sheenSample = getSheenSample(reflection, lod);

    vec3 sheenLight = sheenSample.rgb;
    return sheenLight * sheenColor * brdf;
}


#ifdef MATERIAL_DIFFUSE_TRANSMISSION
// KHR_materials_diffuse_transmission, IBL (glTF-Sample-Renderer: f_diffuse = mix(irradiance(n) * baseColor,
// irradiance(-n) * diffuseTransmissionColor, diffuseTransmission)). Returned as the change to the diffuse term of
// getIBLRadianceLambertian: factor x (k_D(transmissionColor) x E(-n) attenuated - k_D(c_diff) x E(n)), with Glider's
// split-sum diffuse weight k_D(c) = c (1 - FssEss + FmsEms) for both lobes; the multiple-scattering specular FmsEms E(n)
// is not mixed. backIrradiance = getDiffuseLight(-n) (after the volume attenuation, if any).
vec3 getIBLDiffuseTransmissionDelta(vec3 n, vec3 v, float roughness, vec3 diffuseColor, vec3 transmissionColor,
    vec3 backIrradiance, vec3 F0, float specularWeight, float factor)
{
    float NdotV = clampedDot(n, v);
    vec2 brdfSamplePoint = clamp(vec2(NdotV, roughness), vec2(0.0, 0.0), vec2(1.0, 1.0));
    vec2 f_ab = texture(u_GGXLUT, brdfSamplePoint).rg;
    vec3 Fr = max(vec3(1.0 - roughness), F0) - F0;
    vec3 k_S = F0 + Fr * pow(1.0 - NdotV, 5.0);
    vec3 FssEss = specularWeight * k_S * f_ab.x + f_ab.y;
    float Ems = (1.0 - (f_ab.x + f_ab.y));
    vec3 F_avg = specularWeight * (F0 + (1.0 - F0) / 21.0);
    vec3 FmsEms = Ems * FssEss * F_avg / (1.0 - F_avg * Ems);
    vec3 diffuseWeight = vec3(1.0) - FssEss + FmsEms;
    return factor * diffuseWeight * (transmissionColor * backIrradiance - diffuseColor * getDiffuseLight(n));
}
#endif
