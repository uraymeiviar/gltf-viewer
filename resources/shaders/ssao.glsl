// A3.5: screen-space ambient occlusion of the sky light (docs/ASOBO_EXTENSIONS_PHASE_3.md, A3.5).
// u_SsaoTexture (RG32F, full render resolution): x = GTAO visibility (1 = open), y = the linear view depth of the
// prepass surface it belongs to (-1 on the background). It is valid while u_SsaoEnabled != 0.
uniform sampler2D u_SsaoTexture;

// The visibility at this fragment, or 1 when the fragment is not the prepass surface of its pixel (BLEND,
// transmission and other surfaces in front of it, which the prepass does not draw).
float ssaoVisibility(vec3 worldPosition)
{
    vec2 sampleValue = texelFetch(u_SsaoTexture, ivec2(gl_FragCoord.xy), 0).xy;
    float depth = -(u_ViewMatrix * vec4(worldPosition, 1.0)).z;
    if (sampleValue.y < 0.0 || abs(depth - sampleValue.y) > 0.005 * abs(depth) + 1.0e-4) {
        return 1.0;
    }
    return sampleValue.x;
}

// Multiple bounces (Jimenez et al. 2016, eq. 10): the geometric visibility of the SSAO term turned into the
// fraction of sky light that arrives including interreflections off the surrounding surfaces of this albedo,
// the quantity MSFS's authored occlusion stores.
vec3 ssaoMultiBounce(float visibility, vec3 albedo)
{
    vec3 a = 2.0404 * albedo - 0.3324;
    vec3 b = -4.7951 * albedo + 0.6417;
    vec3 c = 2.7552 * albedo + 0.6903;
    return max(vec3(visibility), ((visibility * a + b) * visibility + c) * visibility);
}

// Specular occlusion from ambient occlusion (Lagarde and de Rousiers 2014, "Moving Frostbite to PBR", eq. 32):
// rough lobes take the diffuse occlusion, smooth lobes at normal incidence much less.
float ssaoSpecularOcclusion(float NdotV, float visibility, float alphaRoughness)
{
    return clamp(pow(NdotV + visibility, exp2(-16.0 * alphaRoughness - 1.0)) - 1.0 + visibility, 0.0, 1.0);
}
