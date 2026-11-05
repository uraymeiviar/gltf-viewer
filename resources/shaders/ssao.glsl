// A3.5: screen-space ambient occlusion of the sky light (docs/ASOBO_EXTENSIONS_PHASE_3.md, A3.5).
// u_SsaoTexture (RG32F, full render resolution): x = GTAO visibility (1 = open), y = the linear view depth of the
// prepass surface it belongs to (-1 on the background). It is valid while u_SsaoEnabled != 0.
uniform sampler2D u_SsaoTexture;

// The visibility at this fragment, or 1 when the fragment is not a prepass surface of its neighbourhood (BLEND,
// transmission and other surfaces in front of it, which the prepass does not draw). With MSAA a surface can cover
// only some samples of a pixel whose centre belongs to another surface: the prepass value of the pixel is not its
// own, so it takes the closest depth among the 8 neighbours (returning 1 there draws a light line along silhouettes).
float ssaoVisibility(vec3 worldPosition)
{
    ivec2 center = ivec2(gl_FragCoord.xy);
    vec2 sampleValue = texelFetch(u_SsaoTexture, center, 0).xy;
    float depth = -(u_ViewMatrix * vec4(worldPosition, 1.0)).z;
    float tolerance = 0.005 * abs(depth) + 1.0e-4;
    if (sampleValue.y >= 0.0 && abs(depth - sampleValue.y) <= tolerance) {
        return sampleValue.x;
    }
    ivec2 last = textureSize(u_SsaoTexture, 0) - ivec2(1);
    float closest = tolerance;
    float visibility = 1.0;
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            vec2 neighbour = texelFetch(u_SsaoTexture, clamp(center + ivec2(x, y), ivec2(0), last), 0).xy;
            float difference = abs(depth - neighbour.y);
            if (neighbour.y >= 0.0 && difference <= closest) {
                closest = difference;
                visibility = neighbour.x;
            }
        }
    }
    return visibility;
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
