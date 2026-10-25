//
// This fragment shader defines a reference implementation for Physically Based Shading of
// a microfacet surface material defined by a glTF model.
//
// References:
// [1] Real Shading in Unreal Engine 4
//     http://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf
// [2] Physically Based Shading at Disney
//     http://blog.selfshadow.com/publications/s2012-shading-course/burley/s2012_pbs_disney_brdf_notes_v3.pdf
// [3] README.md - Environment Maps
//     https://github.com/KhronosGroup/glTF-WebGL-PBR/#environment-maps
// [4] "An Inexpensive BRDF Model for Physically based Rendering" by Christophe Schlick
//     https://www.cs.virginia.edu/~jdl/bib/appearance/analytic%20models/schlick94b.pdf
// [5] "KHR_materials_clearcoat"
//     https://github.com/KhronosGroup/glTF/tree/master/extensions/2.0/Khronos/KHR_materials_clearcoat

precision highp float;


#include <camera_block.glsl>
#include <environment_block.glsl>
#include <tonemapping.glsl>
#include <textures.glsl>
#include <functions.glsl>
#include <brdf.glsl>
#include <punctual.glsl>
#include <ibl.glsl>
#include <decal_receiver.glsl>
#include <material_info.glsl>

#ifdef MATERIAL_IRIDESCENCE
#include <iridescence.glsl>
#endif


out vec4 g_finalColor;

#if defined(ASOBO_GLASS_REFRACTION) || defined(ASOBO_WINDSHIELD_SSR)
uniform int u_AsoboGlassCopyValid;
#endif
#ifdef ASOBO_GLASS_REFRACTION
uniform float u_AsoboGlassWidth;

vec3 getAsoboGlassBehind(vec3 n, vec3 v, float ior)
{
    // Parallel slab, width in scene units. Dividing by the normal component gives
    // the travel distance to the exit plane (not simply the length of the ray).
    vec3 ray = refract(-v, n, 1.0 / ior);
    vec3 exitPosition = v_Position + ray * (u_AsoboGlassWidth / max(0.001, abs(dot(ray, n))));
    vec4 clip = u_ViewProjectionMatrix * vec4(exitPosition, 1.0);
    vec4 surfaceClip = u_ViewProjectionMatrix * vec4(v_Position, 1.0);
    // Anchor to the raster pixel, then add only the thickness-dependent displacement.
    // The full-resolution copy shares this viewport's origin, dimensions and Y direction.
    vec2 size = vec2(textureSize(u_TransmissionFramebufferSampler, 0));
    vec2 uv = gl_FragCoord.xy / size;
    uv += 0.5 * (clip.xy / clip.w - surfaceClip.xy / surfaceClip.w);
    vec2 border = 0.5 / size;
    return textureLod(u_TransmissionFramebufferSampler, clamp(uv, border, 1.0 - border), 0.0).rgb;
}
#endif


#ifdef ASOBO_WINDSHIELD_SSR
uniform sampler2D u_SsrDepth;
uniform vec3 u_SsrOptions; // enabled, steps, interior sky strength
uniform float u_SsrAttenuation;
uniform int u_HasInteriorVolume;
uniform vec3 u_InteriorMin;
uniform vec3 u_InteriorMax;

float ssrViewZ(float depth)
{
    // Solve clip.z / clip.w for view Z; works for perspective and orthographic
    // projections, including the XY jitter used by TAA, without a matrix inverse.
    float z = depth * 2.0 - 1.0;
    return (u_ProjectionMatrix[3][2] - z * u_ProjectionMatrix[3][3]) /
           (z * u_ProjectionMatrix[2][3] - u_ProjectionMatrix[2][2]);
}

vec3 ssrPosition(vec2 uv, float depth)
{
    vec4 p = inverse(u_ProjectionMatrix) * vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    return p.xyz / p.w;
}

// Returns hit UV and confidence, not colour. Opaque depth/colour exclude transparents.
// An obstructed/discontinuous ray terminates: never search through it for another hit.
vec4 traceWindshield(vec3 n, vec3 v, vec3 ng)
{
    if (u_SsrOptions.x == 0.0 || u_SsrAttenuation <= 0.0 || u_AsoboGlassCopyValid == 0) return vec4(0);
    ng = dot(ng, v) >= 0.0 ? ng : -ng;
    vec3 reflected = reflect(-v, n);
    if (dot(reflected, ng) <= 0.02) return vec4(0); // reject rays below the physical pane
    vec3 ray = normalize(mat3(u_ViewMatrix) * reflected);
    vec3 origin = (u_ViewMatrix * vec4(v_Position + ng * 0.001, 1.0)).xyz + ray * 0.002;
    float maxTravel = 2.0; // cockpit-scale trace, not a five-metre minimum
    if (ray.z > 0.0) maxTravel = min(maxTravel, (-0.01 - origin.z) / ray.z);
    if (maxTravel <= 0.003) return vec4(0);
    vec3 end = origin + ray * maxTravel;
    vec4 a = u_ProjectionMatrix * vec4(origin, 1.0);
    vec4 b = u_ProjectionMatrix * vec4(end, 1.0);
    vec2 size = vec2(textureSize(u_TransmissionFramebufferSampler, 0));
    vec2 startUV = a.xy / a.w * 0.5 + 0.5;
    vec2 endUV = b.xy / b.w * 0.5 + 0.5;
    vec2 span = (endUV - startUV) * size;
    float pixels = max(abs(span.x), abs(span.y));
    if (pixels < 1.0) return vec4(0);
    float stride = u_SsrOptions.y < 512.0 ? 2.0 : 1.0;
    float increment = stride / pixels;
    vec3 qa = origin / a.w, qb = end / b.w;
    float ka = 1.0 / a.w, kb = 1.0 / b.w;
    float previousS = 0.0;
    float previousDepth = texelFetch(u_SsrDepth, ivec2(clamp(startUV * size, vec2(0), size - 1.0)), 0).r;
    float previousZ = ssrViewZ(previousDepth);
    float previousDelta = previousZ - origin.z;
    for (int i = 1; i <= 1024; ++i) {
        if (i > int(u_SsrOptions.y) || float(i)*stride > 0.65*min(size.x,size.y)) break;
        float t = min(float(i) * increment, 1.0);
        vec2 uv = mix(startUV, endUV, t);
        if (any(lessThanEqual(uv, 1.0 / size)) || any(greaterThanEqual(uv, 1.0 - 1.0 / size))) break;
        vec3 point = mix(qa, qb, t) / mix(ka, kb, t);
        float depth = texelFetch(u_SsrDepth, ivec2(uv * size), 0).r;
        float surfaceZ = ssrViewZ(depth);
        float delta = surfaceZ - point.z;
        if (depth < 0.999999 && delta >= 0.0) {
            // Sky is unknown, not an in-front surface. A silhouette or an already-behind
            // sample is an obstruction, not permission to jump to a more distant object.
            if (previousDepth >= 0.999999 || previousDelta >= 0.0) return vec4(0);
            ivec2 pixel = ivec2(uv * size);
            float dx = texelFetch(u_SsrDepth, pixel + ivec2(1,0), 0).r;
            float dy = texelFetch(u_SsrDepth, pixel + ivec2(0,1), 0).r;
            float diagonal = texelFetch(u_SsrDepth, pixel + ivec2(1,1), 0).r;
            if (max(depth, max(dx,max(dy,diagonal))) >= 0.999999) return vec4(0);
            float slope = max(abs(ssrViewZ(dx) - surfaceZ), abs(ssrViewZ(dy) - surfaceZ));
            if (abs(surfaceZ - previousZ) > max(0.005, slope * 3.0)) return vec4(0);
            vec2 center = (vec2(pixel) + 0.5) / size;
            vec3 hit = ssrPosition(center, depth);
            vec3 hitNormal = normalize(cross(ssrPosition(center + vec2(1,0)/size, dx) - hit,
                                             ssrPosition(center + vec2(0,1)/size, dy) - hit));
            if (dot(hitNormal, -hit) < 0.0) hitNormal = -hitNormal;
            float planeError = abs(dot(ssrPosition(center + vec2(1,1)/size, diagonal) - hit, hitNormal));
            const float thickness = 0.001; // one millimetre; never grows with ray travel
            if (planeError > thickness) return vec4(0); // silhouette/non-planar depth footprint
            float incidence = -dot(ray, hitNormal);
            if (incidence <= 0.05) return vec4(0); // cannot see the back of a front-depth surface
            vec3 previousPoint = mix(qa,qb,previousS) / mix(ka,kb,previousS);
            float front = dot(previousPoint - hit, hitNormal);
            float behind = dot(point - hit, hitNormal);
            if (front <= 0.0 || behind > 0.0) return vec4(0);
            // Refine the continuous local depth plane, not a staircase of nearest depth
            // texels (the latter smears silhouettes and loses foreshortened thin surfaces).
            vec3 lo = previousPoint, hi = point;
            for (int j = 0; j < 5; ++j) {
                vec3 mid = (lo + hi) * 0.5;
                if (dot(mid - hit,hitNormal) > 0.0) lo = mid;
                else hi = mid;
            }
            float travel = dot(hit - origin,hitNormal) / dot(ray,hitNormal);
            point = origin + ray * travel;
            vec4 clip = u_ProjectionMatrix * vec4(point,1.0);
            vec2 hitUV = clip.xy / clip.w * 0.5 + 0.5;
            // An exact plane hit must remain in this depth footprint, never extrapolate
            // a foreground plane across a depth discontinuity to the screen behind it.
            if (any(greaterThan(abs(hitUV*size - (vec2(pixel)+0.5)), vec2(1.5)))) return vec4(0);
            if (travel <= 0.003 || travel >= maxTravel) return vec4(0);
            ivec2 hitPixel = ivec2(hitUV*size);
            float hitDepth = texelFetch(u_SsrDepth,hitPixel,0).r;
            if (hitDepth >= 0.999999) return vec4(0);
            float depthError = abs(dot(ssrPosition((vec2(hitPixel)+0.5)/size,hitDepth)-hit,hitNormal));
            if (depthError > thickness) return vec4(0); // colour texel must belong to this surface

            // The analytic crossing must stay inside the refined bracket. Test the
            // actual hit, not the behind endpoint: its quantization error otherwise
            // creates striped holes on a perfectly continuous foreshortened plane.
            float bracketError = max(dot(lo-point,ray),dot(point-hi,ray));
            if (bracketError > 0.0001) return vec4(0);
            float residual = max(abs(dot(point-hit,hitNormal)),max(0.0,bracketError));
            if (residual > thickness) return vec4(0);
            float edge = min(min(hitUV.x,hitUV.y), min(1.0-hitUV.x,1.0-hitUV.y));
            float screenTravel = length((hitUV-startUV)*size) / min(size.x,size.y);
            float confidence = smoothstep(0.0,0.08,edge) * (1.0-travel/maxTravel);
            confidence *= 1.0-smoothstep(0.15,0.65,screenTravel);
            confidence *= smoothstep(0.05,0.25,incidence) * (1.0-smoothstep(0.25,1.0,max(depthError,max(planeError,residual))/thickness));
            return vec4(hitUV,0.0,confidence);
        }
        previousS = t;
        previousDepth = depth;
        previousDelta = delta;
        previousZ = surfaceZ;
        if (t >= 1.0) break;
    }
    return vec4(0);
}

vec3 windshieldReflection(vec3 fallback, vec3 n, vec3 v, float roughness, vec3 f0, float weight, vec3 ng)
{
    bool inside = u_HasInteriorVolume == 0 ||
        (all(greaterThanEqual(v_Position, u_InteriorMin)) && all(lessThanEqual(v_Position, u_InteriorMax)));
    fallback *= inside ? u_SsrOptions.z : 1.0;
    vec4 hit = traceWindshield(n, v, ng);
    vec2 ab = texture(u_GGXLUT, vec2(clampedDot(n, v), roughness)).rg;
    vec3 ks = f0 + (max(vec3(1.0 - roughness), f0) - f0) * pow(1.0 - clampedDot(n, v), 5.0);
    vec3 radiance = hit.a > 0.0 ? texelFetch(u_TransmissionFramebufferSampler, ivec2(hit.xy * vec2(textureSize(u_TransmissionFramebufferSampler, 0))), 0).rgb : vec3(0);
    vec3 reflected = radiance * weight * (ks * ab.x + ab.y);
    return mix(fallback, reflected, hit.a * clamp(u_SsrAttenuation, 0.0, 1.0));
}
#endif

void main()
{
#ifdef ASOBO_DECAL_RECEIVER
    DecalReceiverInfo decalReceiver = getDecalReceiverInfo();
#endif

    vec4 baseColor = getBaseColor();
#ifdef ASOBO_DECAL_RECEIVER
    baseColor.rgb = applyDecalColor(baseColor.rgb, decalReceiver.baseColor);
#endif

#if ALPHAMODE == ALPHAMODE_OPAQUE
    baseColor.a = 1.0;
#endif

    vec3 v = normalize(u_Camera - v_Position);
    NormalInfo normalInfo = getNormalInfo(v);
#ifdef ASOBO_DECAL_RECEIVER
    // A2.2: the decal normal is world space; mix it with the receiver's shading normal
    // and renormalize before any NdotV term is derived. The explicit branch keeps a
    // receiver with T == 1 bit-for-bit unchanged.
    if (decalReceiver.normal.a < 1.0) {
        normalInfo.n = normalize(decalReceiver.normal.rgb + decalReceiver.normal.a * normalInfo.n);
    }
#endif
    vec3 n = normalInfo.n;
    vec3 t = normalInfo.t;
    vec3 b = normalInfo.b;

    float NdotV = clampedDot(n, v);
    float TdotV = clampedDot(t, v);
    float BdotV = clampedDot(b, v);

    MaterialInfo materialInfo;
    materialInfo.baseColor = baseColor.rgb;
    
    // The default index of refraction of 1.5 yields a dielectric normal incidence reflectance of 0.04.
    materialInfo.ior = 1.5;
    materialInfo.f0 = vec3(0.04);
    materialInfo.specularWeight = 1.0;

    // If the MR debug output is selected, we have to enforce evaluation of the non-iridescence BRDF functions.
#if DEBUG == DEBUG_METALLIC_ROUGHNESS
#undef MATERIAL_IRIDESCENCE
#endif

#ifdef MATERIAL_IOR
    materialInfo = getIorInfo(materialInfo);
#endif

#ifdef MATERIAL_SPECULARGLOSSINESS
    materialInfo = getSpecularGlossinessInfo(materialInfo);
#endif

#ifdef MATERIAL_METALLICROUGHNESS
    materialInfo = getMetallicRoughnessInfo(materialInfo);
#endif

#ifdef MATERIAL_SHEEN
    materialInfo = getSheenInfo(materialInfo);
#endif

#ifdef MATERIAL_CLEARCOAT
    materialInfo = getClearCoatInfo(materialInfo, normalInfo);
#endif

#ifdef MATERIAL_SPECULAR
    materialInfo = getSpecularInfo(materialInfo);
#endif

#ifdef MATERIAL_TRANSMISSION
    materialInfo = getTransmissionInfo(materialInfo);
#endif

#ifdef MATERIAL_VOLUME
    materialInfo = getVolumeInfo(materialInfo);
#endif

#ifdef MATERIAL_IRIDESCENCE
    materialInfo = getIridescenceInfo(materialInfo);
#endif

    materialInfo.perceptualRoughness = clamp(materialInfo.perceptualRoughness, 0.0, 1.0);
    materialInfo.metallic = clamp(materialInfo.metallic, 0.0, 1.0);

    // Roughness is authored as perceptual roughness; as is convention,
    // convert to material roughness by squaring the perceptual roughness.
    materialInfo.alphaRoughness = materialInfo.perceptualRoughness * materialInfo.perceptualRoughness;

    // Compute reflectance.
    float reflectance = max(max(materialInfo.f0.r, materialInfo.f0.g), materialInfo.f0.b);

    // Anything less than 2% is physically impossible and is instead considered to be shadowing. Compare to "Real-Time-Rendering" 4th editon on page 325.
    materialInfo.f90 = vec3(1.0);

    // LIGHTING
    vec3 f_specular = vec3(0.0);
    vec3 f_diffuse = vec3(0.0);
    vec3 f_emissive = vec3(0.0);
    vec3 f_clearcoat = vec3(0.0);
    vec3 f_sheen = vec3(0.0);
    vec3 f_transmission = vec3(0.0);

    float albedoSheenScaling = 1.0;

#ifdef MATERIAL_IRIDESCENCE
    vec3 iridescenceFresnel = materialInfo.f0;
    vec3 iridescenceF0 = materialInfo.f0;

    if (materialInfo.iridescenceThickness == 0.0) {
        materialInfo.iridescenceFactor = 0.0;
    }

    if (materialInfo.iridescenceFactor > 0.0) {
        iridescenceFresnel = evalIridescence(1.0, materialInfo.iridescenceIor, NdotV, materialInfo.iridescenceThickness, materialInfo.f0);
        iridescenceF0 = Schlick_to_F0(iridescenceFresnel, NdotV);
    }
#endif

    // Calculate lighting contribution from image based lighting source (IBL)
#ifdef USE_IBL
#ifdef MATERIAL_IRIDESCENCE
    f_specular += getIBLRadianceGGXIridescence(n, v, materialInfo.perceptualRoughness, materialInfo.f0, iridescenceFresnel, materialInfo.iridescenceFactor, materialInfo.specularWeight);
    f_diffuse += getIBLRadianceLambertianIridescence(n, v, materialInfo.perceptualRoughness, materialInfo.c_diff, materialInfo.f0, iridescenceF0, materialInfo.iridescenceFactor, materialInfo.specularWeight);
#else
#ifdef ASOBO_WINDSHIELD_SSR
    f_specular += windshieldReflection(getIBLRadianceGGX(n, v, materialInfo.perceptualRoughness, materialInfo.f0, materialInfo.specularWeight),
        n, v, materialInfo.perceptualRoughness, materialInfo.f0, materialInfo.specularWeight, normalInfo.ng);
#else
    f_specular += getIBLRadianceGGX(n, v, materialInfo.perceptualRoughness, materialInfo.f0, materialInfo.specularWeight);
#endif
    f_diffuse += getIBLRadianceLambertian(n, v, materialInfo.perceptualRoughness, materialInfo.c_diff, materialInfo.f0, materialInfo.specularWeight);
#endif

#ifdef MATERIAL_CLEARCOAT
    f_clearcoat += getIBLRadianceGGX(materialInfo.clearcoatNormal, v, materialInfo.clearcoatRoughness, materialInfo.clearcoatF0, 1.0);
#endif

#ifdef MATERIAL_SHEEN
    f_sheen += getIBLRadianceCharlie(n, v, materialInfo.sheenRoughnessFactor, materialInfo.sheenColorFactor);
#endif
#endif

#if defined(ASOBO_WINDSHIELD_SSR) && !defined(USE_IBL)
    f_specular += windshieldReflection(vec3(0), n, v, materialInfo.perceptualRoughness, materialInfo.f0, materialInfo.specularWeight, normalInfo.ng);
#endif
#if defined(MATERIAL_TRANSMISSION) && defined(USE_IBL)
    f_transmission += getIBLVolumeRefraction(
        n, v,
        materialInfo.perceptualRoughness,
        materialInfo.c_diff, materialInfo.f0, materialInfo.f90,
        v_Position, u_ModelMatrix, u_ViewMatrix, u_ProjectionMatrix,
        materialInfo.ior, materialInfo.thickness, materialInfo.attenuationColor, materialInfo.attenuationDistance);
#endif

    float ao = 1.0;
    // Apply optional PBR terms for additional (optional) shading
#ifdef HAS_OCCLUSION_MAP
    ao = texture(u_OcclusionSampler,  getOcclusionUV()).r;
#ifdef ASOBO_ASSET_OPTIMIZED
    // A1.12: the packaged COMP texture stores AO as sqrt(linear AO).
    ao *= ao;
#endif
    f_diffuse = mix(f_diffuse, f_diffuse * ao, u_OcclusionStrength);
    // apply ambient occlusion to all lighting that is not punctual
    f_specular = mix(f_specular, f_specular * ao, u_OcclusionStrength);
    f_sheen = mix(f_sheen, f_sheen * ao, u_OcclusionStrength);
    f_clearcoat = mix(f_clearcoat, f_clearcoat * ao, u_OcclusionStrength);
#endif
#ifdef ASOBO_DECAL_RECEIVER
    // A2.2: the decal AO multiplies the IBL terms with strength 1, also when the receiver
    // has no occlusion map. The debug Occlusion view shows the combined product.
    if (decalReceiver.occlusion < 1.0) {
        ao *= decalReceiver.occlusion;
        f_diffuse *= decalReceiver.occlusion;
        f_specular *= decalReceiver.occlusion;
        f_sheen *= decalReceiver.occlusion;
        f_clearcoat *= decalReceiver.occlusion;
    }
#endif

#ifdef USE_PUNCTUAL
    for (int i = 0; i < LIGHT_COUNT; ++i)
    {
        Light light = u_Lights[i];

        vec3 pointToLight;
        if (light.type != LightType_Directional)
        {
            pointToLight = light.position - v_Position;
        }
        else
        {
            pointToLight = -light.direction;
        }

        // BSTF
        vec3 l = normalize(pointToLight);   // Direction from surface point to light
        vec3 h = normalize(l + v);          // Direction of the vector between l and v, called halfway vector
        float NdotL = clampedDot(n, l);
        float NdotV = clampedDot(n, v);
        float NdotH = clampedDot(n, h);
        float LdotH = clampedDot(l, h);
        float VdotH = clampedDot(v, h);
        if (NdotL > 0.0 || NdotV > 0.0)
        {
            // Calculation of analytical light
            // https://github.com/KhronosGroup/glTF/tree/master/specification/2.0#acknowledgments AppendixB
            vec3 intensity = getLighIntensity(light, pointToLight);
#ifdef MATERIAL_IRIDESCENCE
            f_diffuse += intensity * NdotL *  BRDF_lambertianIridescence(materialInfo.f0, materialInfo.f90, iridescenceFresnel, materialInfo.iridescenceFactor, materialInfo.c_diff, materialInfo.specularWeight, VdotH);
            f_specular += intensity * NdotL * BRDF_specularGGXIridescence(materialInfo.f0, materialInfo.f90, iridescenceFresnel, materialInfo.alphaRoughness, materialInfo.iridescenceFactor, materialInfo.specularWeight, VdotH, NdotL, NdotV, NdotH);
#else
            f_diffuse += intensity * NdotL *  BRDF_lambertian(materialInfo.f0, materialInfo.f90, materialInfo.c_diff, materialInfo.specularWeight, VdotH);
            f_specular += intensity * NdotL * BRDF_specularGGX(materialInfo.f0, materialInfo.f90, materialInfo.alphaRoughness, materialInfo.specularWeight, VdotH, NdotL, NdotV, NdotH);
#endif

#ifdef MATERIAL_SHEEN
            f_sheen += intensity * getPunctualRadianceSheen(materialInfo.sheenColorFactor, materialInfo.sheenRoughnessFactor, NdotL, NdotV, NdotH);
            albedoSheenScaling = min(1.0 - max3(materialInfo.sheenColorFactor) * albedoSheenScalingLUT(NdotV, materialInfo.sheenRoughnessFactor),
                1.0 - max3(materialInfo.sheenColorFactor) * albedoSheenScalingLUT(NdotL, materialInfo.sheenRoughnessFactor));
#endif

#ifdef MATERIAL_CLEARCOAT
            f_clearcoat += intensity * getPunctualRadianceClearCoat(materialInfo.clearcoatNormal, v, l, h, VdotH,
                materialInfo.clearcoatF0, materialInfo.clearcoatF90, materialInfo.clearcoatRoughness);
#endif
        }

        // BDTF
#ifdef MATERIAL_TRANSMISSION
        // If the light ray travels through the geometry, use the point it exits the geometry again.
        // That will change the angle to the light source, if the material refracts the light ray.
        vec3 transmissionRay = getVolumeTransmissionRay(n, v, materialInfo.thickness, materialInfo.ior, u_ModelMatrix);
        pointToLight -= transmissionRay;
        l = normalize(pointToLight);

        vec3 intensity = getLighIntensity(light, pointToLight);
        vec3 transmittedLight = intensity * getPunctualRadianceTransmission(n, v, l, materialInfo.alphaRoughness, materialInfo.f0, materialInfo.f90, materialInfo.c_diff, materialInfo.ior);

#ifdef MATERIAL_VOLUME
        transmittedLight = applyVolumeAttenuation(transmittedLight, length(transmissionRay), materialInfo.attenuationColor, materialInfo.attenuationDistance);
#endif

        f_transmission += transmittedLight;
#endif
    }
#endif

    f_emissive = u_EmissiveFactor;
#ifdef MATERIAL_EMISSIVE_STRENGTH
    f_emissive *= u_EmissiveStrength;
#endif
#ifdef HAS_EMISSIVE_MAP
    f_emissive *= texture(u_EmissiveSampler, getEmissiveUV()).rgb;
#endif
#ifdef ASOBO_DECAL_RECEIVER
    if (decalReceiver.emissive.a < 1.0) {
        f_emissive = decalReceiver.emissive.rgb + decalReceiver.emissive.a * f_emissive;
    }
#endif

    // Layer blending

    float clearcoatFactor = 0.0;
    vec3 clearcoatFresnel = vec3(0);

#ifdef MATERIAL_CLEARCOAT
    clearcoatFactor = materialInfo.clearcoatFactor;
    clearcoatFresnel = F_Schlick(materialInfo.clearcoatF0, materialInfo.clearcoatF90, clampedDot(materialInfo.clearcoatNormal, v));
    f_clearcoat = f_clearcoat * clearcoatFactor;
#endif

#ifdef MATERIAL_TRANSMISSION
    vec3 diffuse = mix(f_diffuse, f_transmission, materialInfo.transmissionFactor);
#else
    vec3 diffuse = f_diffuse;
#endif

    vec3 color = vec3(0);
#ifdef MATERIAL_UNLIT
    color = baseColor.rgb;
#else
    color = f_emissive + diffuse + f_specular;
    color = f_sheen + color * albedoSheenScaling;
    color = color * (1.0 - clearcoatFactor * clearcoatFresnel) + f_clearcoat;
#endif

#if DEBUG == DEBUG_NONE || DEBUG == DEBUG_SSR || DEBUG == DEBUG_SSR_RAY

#if ALPHAMODE == ALPHAMODE_MASK
    // Late discard to avoid samplig artifacts. See https://github.com/KhronosGroup/glTF-Sample-Viewer/issues/267
    if (baseColor.a < u_AlphaCutoff)
    {
        discard;
    }
    baseColor.a = 1.0;
#endif

#ifdef ASOBO_GLASS_REFLECTION
    // Existing GGX IBL and punctual terms already include Fresnel. Coverage applies
    // only to the base layer, never to the reflection. Keep debug views unchanged.
    float coverage = clamp(baseColor.a, 0.0, 1.0);
#ifdef MATERIAL_UNLIT
    color = baseColor.rgb * coverage;
#else
    color = (f_emissive + diffuse) * coverage + f_specular;
    color = f_sheen + color * albedoSheenScaling;
    color = color * (1.0 - clearcoatFactor * clearcoatFresnel) + f_clearcoat;
#endif
#ifdef ASOBO_GLASS_REFRACTION
    if (u_AsoboGlassCopyValid != 0) {
        color += getAsoboGlassBehind(n, v, materialInfo.ior) * (1.0 - coverage) * baseColor.rgb;
        // This is the complete scene colour, not a translucent layer. ImGui must
        // not multiply it by glass coverage again when it displays the viewport.
        baseColor.a = 1.0;
    }
#endif
#endif

#ifdef LINEAR_OUTPUT
    g_finalColor = vec4(color.rgb, baseColor.a);
#else
    g_finalColor = vec4(toneMap(color), baseColor.a);
#endif

#else
    // In case of missing data for a debug view, render a checkerboard.
    g_finalColor = vec4(1.0);
    {
        float frequency = 0.02;
        float gray = 0.9;

        vec2 v1 = step(0.5, fract(frequency * gl_FragCoord.xy));
        vec2 v2 = step(0.5, vec2(1.0) - fract(frequency * gl_FragCoord.xy));
        g_finalColor.rgb *= gray + v1.x * v1.y + v2.x * v2.y;
    }
#endif

    // Debug views:

    // Generic:
#if DEBUG == DEBUG_UV_0 && defined(HAS_TEXCOORD_0_VEC2)
    g_finalColor.rgb = vec3(v_texcoord_0, 0);
#endif
#if DEBUG == DEBUG_UV_1 && defined(HAS_TEXCOORD_1_VEC2)
    g_finalColor.rgb = vec3(v_texcoord_1, 0);
#endif
#if DEBUG == DEBUG_NORMAL_TEXTURE && defined(HAS_NORMAL_MAP)
    g_finalColor.rgb = (normalInfo.ntex + 1.0) / 2.0;
#endif
#if DEBUG == DEBUG_NORMAL_SHADING
    g_finalColor.rgb = (n + 1.0) / 2.0;
#endif
#if DEBUG == DEBUG_NORMAL_GEOMETRY
    g_finalColor.rgb = (normalInfo.ng + 1.0) / 2.0;
#endif
#if DEBUG == DEBUG_TANGENT
    g_finalColor.rgb = (normalInfo.t + 1.0) / 2.0;
#endif
#if DEBUG == DEBUG_BITANGENT
    g_finalColor.rgb = (normalInfo.b + 1.0) / 2.0;
#endif
#if DEBUG == DEBUG_ALPHA
    g_finalColor.rgb = linearTosRGB(vec3(baseColor.a));
#endif
#if DEBUG == DEBUG_OCCLUSION && defined(HAS_OCCLUSION_MAP)
    g_finalColor.rgb = linearTosRGB(vec3(ao));
#elif DEBUG == DEBUG_OCCLUSION && defined(ASOBO_DECAL_RECEIVER)
    // A2.2: show the decal AO product even when the receiver has no occlusion map.
    if (u_AsoboDecalReceiver != 0) {
        g_finalColor.rgb = linearTosRGB(vec3(ao));
    }
#endif
#if DEBUG == DEBUG_EMISSIVE
    g_finalColor.rgb = linearTosRGB(f_emissive);
#endif

    // MR:
#ifdef MATERIAL_METALLICROUGHNESS
#if DEBUG == DEBUG_METALLIC_ROUGHNESS
    g_finalColor.rgb = linearTosRGB(f_diffuse + f_specular);
#endif
#if DEBUG == DEBUG_METALLIC
    g_finalColor.rgb = linearTosRGB(vec3(materialInfo.metallic));
#endif
#if DEBUG == DEBUG_ROUGHNESS
    g_finalColor.rgb = linearTosRGB(vec3(materialInfo.perceptualRoughness));
#endif
#if DEBUG == DEBUG_BASE_COLOR
    g_finalColor.rgb = linearTosRGB(materialInfo.baseColor);
#endif
#endif

    // Clearcoat:
#ifdef MATERIAL_CLEARCOAT
#if DEBUG == DEBUG_CLEARCOAT
    g_finalColor.rgb = linearTosRGB(f_clearcoat);
#endif
#if DEBUG == DEBUG_CLEARCOAT_FACTOR
    g_finalColor.rgb = linearTosRGB(vec3(materialInfo.clearcoatFactor));
#endif
#if DEBUG == DEBUG_CLEARCOAT_ROUGHNESS
    g_finalColor.rgb = linearTosRGB(vec3(materialInfo.clearcoatRoughness));
#endif
#if DEBUG == DEBUG_CLEARCOAT_NORMAL
    g_finalColor.rgb = (materialInfo.clearcoatNormal + vec3(1)) / 2.0;
#endif
#endif

    // Sheen:
#ifdef MATERIAL_SHEEN
#if DEBUG == DEBUG_SHEEN
    g_finalColor.rgb = linearTosRGB(f_sheen);
#endif
#if DEBUG == DEBUG_SHEEN_COLOR
    g_finalColor.rgb = linearTosRGB(materialInfo.sheenColorFactor);
#endif
#if DEBUG == DEBUG_SHEEN_ROUGHNESS
    g_finalColor.rgb = linearTosRGB(vec3(materialInfo.sheenRoughnessFactor));
#endif
#endif

    // Specular:
#ifdef MATERIAL_SPECULAR
#if DEBUG == DEBUG_SPECULAR
    g_finalColor.rgb = linearTosRGB(f_specular);
#endif
#if DEBUG == DEBUG_SPECULAR_FACTOR
    g_finalColor.rgb = vec3(materialInfo.specularWeight);
#endif

#if DEBUG == DEBUG_SPECULAR_COLOR
vec3 specularTexture = vec3(1.0);
#ifdef HAS_SPECULAR_COLOR_MAP
    specularTexture.rgb = texture(u_SpecularColorSampler, getSpecularColorUV()).rgb;
#endif
    g_finalColor.rgb = u_KHR_materials_specular_specularColorFactor * specularTexture.rgb;
#endif
#endif

    // Transmission, Volume:
#ifdef MATERIAL_TRANSMISSION
#if DEBUG == DEBUG_TRANSMISSION_VOLUME
    g_finalColor.rgb = linearTosRGB(f_transmission);
#endif
#if DEBUG == DEBUG_TRANSMISSION_FACTOR
    g_finalColor.rgb = linearTosRGB(vec3(materialInfo.transmissionFactor));
#endif
#endif
#ifdef MATERIAL_VOLUME
#if DEBUG == DEBUG_VOLUME_THICKNESS
    g_finalColor.rgb = linearTosRGB(vec3(materialInfo.thickness));
#endif
#endif

    // Iridescence:
#ifdef MATERIAL_IRIDESCENCE
#if DEBUG == DEBUG_IRIDESCENCE
    g_finalColor.rgb = linearTosRGB(iridescenceFresnel);
#endif
#if DEBUG == DEBUG_IRIDESCENCE_FACTOR
    g_finalColor.rgb = linearTosRGB(vec3(materialInfo.iridescenceFactor));
#endif
#if DEBUG == DEBUG_IRIDESCENCE_THICKNESS
    g_finalColor.rgb = linearTosRGB(vec3(materialInfo.iridescenceThickness / 1200.0));
#endif
#endif
#if DEBUG == DEBUG_SSR && defined(ASOBO_WINDSHIELD_SSR)
    vec4 ssrDebug = traceWindshield(n, v, normalInfo.ng);
    g_finalColor = vec4(ssrDebug.xy, ssrDebug.a, 1.0); // R/G hit UV; B confidence; black miss
#endif
#if DEBUG == DEBUG_SSR_RAY && defined(ASOBO_WINDSHIELD_SSR)
    vec3 ng = dot(normalInfo.ng,v) >= 0.0 ? normalInfo.ng : -normalInfo.ng;
    vec3 reflected = reflect(-v,n);
    g_finalColor = vec4(normalize(mat3(u_ViewMatrix)*reflected)*0.5+0.5, 1.0);
    if (dot(reflected,ng) <= 0.02) g_finalColor = vec4(1,0,1,1); // invalid pane hemisphere
#endif

}
