const float M_PI = 3.141592653589793;


in vec3 v_Position;


#ifdef HAS_NORMAL_VEC3
#ifdef HAS_TANGENT_VEC4
in mat3 v_TBN;
#else
in vec3 v_Normal;
#endif
#endif


#ifdef HAS_COLOR_0_VEC3
in vec3 v_Color;
#endif
#ifdef HAS_COLOR_0_VEC4
in vec4 v_Color;
#endif


vec4 getVertexColor()
{
   vec4 color = vec4(1.0);

#ifdef HAS_COLOR_0_VEC3
    color.rgb = v_Color.rgb;
#endif
#ifdef HAS_COLOR_0_VEC4
    color = v_Color;
#endif

   return color;
}


struct NormalInfo {
    vec3 ng;   // Geometry normal
    vec3 t;    // Geometry tangent
    vec3 b;    // Geometry bitangent
    vec3 n;    // Shading normal
    vec3 ntex; // Normal from texture, scaling is accounted for.
};


// Samples a tangent-space normal map and decodes it to [-1, 1]. ASOBO_normal_map_convention
// (A1.2) is asset-level: DirectX stores green inverted relative to glTF's OpenGL convention,
// and the value arrives in EnvironmentBlock because the viewport switch is per viewport.
// Every normal-map sample goes through here so later phases reuse the same convention.
vec3 sampleTangentSpaceNormal(sampler2D normalSampler, vec2 uv)
{
    vec3 texel = texture(normalSampler, uv).rgb;
    if (u_AsoboNormalMapDirectX != 0) {
        texel.g = 1.0 - texel.g;
    }
    return texel * 2.0 - vec3(1.0);
}


// BC5 SNORM normal maps (MSFS *_NORM.PNG.KTX2) store signed X and Y directly and leave Z
// to be rebuilt. The A1.2 DirectX convention still applies, but the signed green flip is
// an arithmetic negation instead of 1.0 - g.
vec3 sampleTangentSpaceNormalRG(sampler2D normalSampler, vec2 uv)
{
    vec2 texel = texture(normalSampler, uv).rg;
    if (u_AsoboNormalMapDirectX != 0) {
        texel.g = -texel.g;
    }
    return vec3(texel, sqrt(max(0.0, 1.0 - dot(texel, texel))));
}


float clampedDot(vec3 x, vec3 y)
{
    return clamp(dot(x, y), 0.0, 1.0);
}


float max3(vec3 v)
{
    return max(max(v.x, v.y), v.z);
}


float sq(float t)
{
    return t * t;
}

vec2 sq(vec2 t)
{
    return t * t;
}

vec3 sq(vec3 t)
{
    return t * t;
}

vec4 sq(vec4 t)
{
    return t * t;
}


float applyIorToRoughness(float roughness, float ior)
{
    // Scale roughness with IOR so that an IOR of 1.0 results in no microfacet refraction and
    // an IOR of 1.5 results in the default amount of microfacet refraction.
    return roughness * clamp(ior * 2.0 - 2.0, 0.0, 1.0);
}
