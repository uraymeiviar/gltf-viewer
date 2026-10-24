layout(location = 0) out vec4 g_finalColor;
layout(location = 1) out vec3 g_pickingColor;

#include <environment_block.glsl>
#include <functions.glsl>
#include <textures.glsl>

uniform vec3 u_Camera; // position
uniform vec4 u_BaseColorFactor;
uniform vec3 u_PickingColor = vec3(0);

// Get normal, tangent and bitangent vectors.
NormalInfo getNormalInfo(vec3 v)
{
    vec2 UV = getNormalUV();
    vec3 uv_dx = dFdx(vec3(UV, 0.0));
    vec3 uv_dy = dFdy(vec3(UV, 0.0));

    vec3 t_ = (uv_dy.t * dFdx(v_Position) - uv_dx.t * dFdy(v_Position)) / (uv_dx.s * uv_dy.t - uv_dy.s * uv_dx.t);

    vec3 n, t, b, ng;

    // Compute geometrical TBN:
#ifdef HAS_NORMAL_VEC3
#ifdef HAS_TANGENT_VEC4
    // Trivial TBN computation, present as vertex attribute.
    // Normalize eigenvectors as matrix is linearly interpolated.
    t = normalize(v_TBN[0]);
    b = normalize(v_TBN[1]);
    ng = normalize(v_TBN[2]);
#else
    // Normals are either present as vertex attributes or approximated.
    ng = normalize(v_Normal);
    t = normalize(t_ - ng * dot(ng, t_));
    b = cross(ng, t);
#endif
#else
    ng = normalize(cross(dFdx(v_Position), dFdy(v_Position)));
    t = normalize(t_ - ng * dot(ng, t_));
    b = cross(ng, t);
#endif

    // For a back-facing surface, the tangential basis vectors are negated.
    if (gl_FrontFacing == false) {
        t *= -1.0;
        b *= -1.0;
        ng *= -1.0;
    }

    // Compute normals:
    NormalInfo info;
    info.ng = ng;
#ifdef HAS_NORMAL_MAP
#ifdef NORMAL_TEXTURE_RG_SNORM
    info.ntex = sampleTangentSpaceNormalRG(u_NormalSampler, UV);
#else
    info.ntex = sampleTangentSpaceNormal(u_NormalSampler, UV);
#endif
    info.ntex *= vec3(u_NormalScale, u_NormalScale, 1.0);
    info.ntex = normalize(info.ntex);
    info.n = normalize(mat3(t, b, ng) * info.ntex);
#else
    info.n = ng;
#endif
    info.t = t;
    info.b = b;
    return info;
}

void main()
{
    g_pickingColor = u_PickingColor;

    vec3 view = normalize(u_Camera - v_Position);
    NormalInfo normalInfo = getNormalInfo(view);
    vec3 n = normalInfo.n;
    vec3 t = normalInfo.t;
    vec3 b = normalInfo.b;
    float lightFactor = dot(view, n) * 0.5 + 0.5;
    vec4 baseColor = u_BaseColorFactor;

#if defined(MATERIAL_METALLICROUGHNESS) && defined(HAS_BASE_COLOR_MAP)
    baseColor *= texture(u_BaseColorSampler, getBaseColorUV());
#endif

    // use fixed gamma value for gizmos
    g_finalColor = vec4(pow(baseColor.rgb, vec3(1.0 / 2.2)), baseColor.a);
    g_finalColor.rgb = g_finalColor.rgb * lightFactor;
}
