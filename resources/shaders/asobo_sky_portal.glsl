// A3.7: ASOBO_sky_portal, window area lights that bring the sky into a cabin (docs/ASOBO_EXTENSIONS_PHASE_3.md,
// A3.7). Each portal is a disc at a window facing into the cabin. Its radiance towards a receiver is the
// environment seen through the window from that receiver (the prefiltered GGX cube map at a rough mip in the
// direction receiver -> disc centre, rotated like the IBL), so a receiver on the portal's axis sees the sky in the
// portal's outward direction. The irradiance uses the disc form factor of Lagarde and de Rousiers 2014 ("Moving
// Frostbite to PBR", disk lights without horizon clipping): E = pi L cos(emit) cos(receive) r^2 / (d^2 + r^2),
// exact on the axis, times the cone falloff between the outer and the inner cone angle. There is no visibility
// between portal and receiver (the occlusion weighting in pbr.frag stands in for it) and no specular term.
#ifdef ASOBO_SKY_PORTAL
const float kSkyPortalRoughness = 0.6; // the mip the window sky is read at (perceptual roughness of the GGX chain)

vec3 skyPortalIrradiance(vec3 position, vec3 n)
{
    vec3 irradiance = vec3(0.0);
    float lod = kSkyPortalRoughness * float(u_MipCount - 1);
    for (int i = 0; i < 8; ++i) {
        if (i >= u_SkyPortalCount) {
            break;
        }
        vec4 positionRadius = u_SkyPortalPositionRadius[i];
        vec4 normalCosInner = u_SkyPortalNormalCosInner[i];
        float cosOuter = u_SkyPortalCosOuter[i].x;
        vec3 toPortal = positionRadius.xyz - position;
        float distanceSquared = max(dot(toPortal, toPortal), 1.0e-8);
        vec3 l = toPortal * inversesqrt(distanceSquared);
        // The receiver must be in front of the disc (inside the cabin) and face it.
        float cosEmit = dot(normalCosInner.xyz, -l);
        float cosReceive = dot(n, l);
        if (cosEmit <= 0.0 || cosReceive <= 0.0) {
            continue;
        }
        float cone = normalCosInner.w > cosOuter + 1.0e-4 ? smoothstep(cosOuter, normalCosInner.w, cosEmit)
                                                           : step(cosOuter, cosEmit);
        float radiusSquared = positionRadius.w * positionRadius.w;
        float formFactor = cosEmit * cosReceive * radiusSquared / (distanceSquared + radiusSquared);
        vec3 sky = textureLod(u_GGXEnvSampler, u_EnvRotation * l, lod).rgb * u_EnvIntensity;
        irradiance += M_PI * sky * (formFactor * cone);
    }
    return irradiance;
}
#endif
