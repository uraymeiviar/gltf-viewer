// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include <animation.glsl>

in vec3 a_position;
out vec3 v_Position;

#ifdef HAS_NORMAL_VEC3
in vec3 a_normal;
#endif

#ifdef HAS_TANGENT_VEC4
in vec4 a_tangent;
#endif

#ifdef HAS_NORMAL_VEC3
out vec3 v_Normal;

#ifdef HAS_TANGENT_VEC4
out mat3 v_TBN;
#endif // !HAS_TANGENT_VEC4

#endif // !HAS_NORMAL_VEC3

#ifdef HAS_TEXCOORD_0_VEC2
in vec2 a_texcoord_0;
#endif

#ifdef HAS_TEXCOORD_1_VEC2
in vec2 a_texcoord_1;
#endif

out vec2 v_texcoord_0;
out vec2 v_texcoord_1;

#ifdef HAS_COLOR_0_VEC3
in vec3 a_color_0;
out vec3 v_Color;
#endif

#ifdef HAS_COLOR_0_VEC4
in vec4 a_color_0;
out vec4 v_Color;
#endif

uniform mat4 u_ViewMatrix;
uniform mat4 u_ProjectionMatrix;
uniform mat4 u_ViewProjectionMatrix;
uniform mat4 u_ModelMatrix;
uniform vec3 u_Camera;
uniform mat4 u_NormalMatrix;
uniform float u_ExtrusionFactor = 0.f;

uniform ivec2 u_viewportResolution;

#ifdef IS_SCREEN_SPACE_POSITIONED
uniform vec2 u_screenSpacePosition;
#endif // IS_SCREEN_SPACE_POSITIONED

uniform float u_orthographicScale = 1.0;

#ifdef IS_FIXEDSIZE
uniform float u_fixedSizeScale;
#endif // IS_FIXEDSIZE

vec4 getPosition()
{
    vec4 pos = vec4(a_position, 1.0);

#ifdef USE_MORPHING
    pos += getTargetPosition(gl_VertexID);
#endif

#ifdef USE_SKINNING
    pos = getSkinningMatrix() * pos;
#endif

    return pos;
}


vec3 getNormal()
{
#ifdef HAS_NORMAL_VEC3
    vec3 normal = a_normal;

#ifdef USE_MORPHING
        normal += getTargetNormal(gl_VertexID);
#endif

#ifdef USE_SKINNING
        normal = mat3(getSkinningNormalMatrix()) * normal;
#endif

    return normalize(normal);
#endif

    return vec3(0);
}


#ifdef HAS_TANGENT_VEC4
vec3 getTangent()
{
    vec3 tangent = a_tangent.xyz;

#ifdef USE_MORPHING
        tangent += getTargetTangent(gl_VertexID);    
#endif

#ifdef USE_SKINNING
        tangent = mat3(getSkinningMatrix()) * tangent;
#endif

    return normalize(tangent);
}
#endif

struct TransformInfo
{
    mat4 normalMatrix;
    mat4 modelMatrix;
    mat4 viewMatrix;
    mat4 projectionMatrix;
    mat4 viewProjectionMatrix;
};

#ifdef IS_BILLBOARD
TransformInfo applyBillboarding(TransformInfo info)
{
    // construct the billboard matrix
    vec3 right = normalize(info.viewMatrix[0].xyz);
    vec3 up = normalize(info.viewMatrix[1].xyz);
    vec3 look = normalize(info.viewMatrix[2].xyz);
    mat4 billboardMatrix = mat4(transpose(mat3(right, up, look)));

    // nuke the upper left 3x3 of the model matrix with the billboard matrix
    vec4 modelTranslation = info.modelMatrix[3];
    info.modelMatrix =  billboardMatrix;
    info.modelMatrix[3] = modelTranslation;

    // recreate the normal matrix with the updated transform
    info.normalMatrix = transpose(inverse(info.modelMatrix));
    
    return info;
}
#endif

#ifdef IS_SCREEN_SPACE_POSITIONED
TransformInfo translateToClipSpaceOrigin(TransformInfo info)
{
    // transform the object to the center of the viewport (to avoid weird projections)
    vec4 clipSpaceOrigin = vec4(0.0, 0.0, 0.0, 1.0);
    vec4 worldPos = inverse(info.viewProjectionMatrix) * clipSpaceOrigin;
    info.modelMatrix[3] = worldPos;
    return info;
}
#endif

#ifdef IS_SCREEN_SPACE_POSITIONED
vec4 applyScreenSpacePosition(vec4 clipSpacePosition)
{
    // construct the world space position that corresponds to the 
    // specified clip space position
    vec2 screenSpacePosition = u_screenSpacePosition;
    vec2 viewportResolution = u_viewportResolution;
    if(screenSpacePosition.x < 0.0)
    {
        screenSpacePosition.x += viewportResolution.x;
    }
    if(screenSpacePosition.y < 0.0)
    {
        screenSpacePosition.y += viewportResolution.y;
    }

    vec2 clipPos = (2.0 * screenSpacePosition / viewportResolution) - vec2(1.0);
    clipSpacePosition += vec4(clipPos, 0.0, 0.0);
    clipSpacePosition.w = 1.0f;

    return clipSpacePosition;
}
#endif

void main()
{
    TransformInfo info;
    info.normalMatrix = u_NormalMatrix;
    info.modelMatrix = u_ModelMatrix;
    info.viewMatrix = u_ViewMatrix;
    info.projectionMatrix = u_ProjectionMatrix;
    info.viewProjectionMatrix = u_ViewProjectionMatrix;

    #ifdef IS_SCREEN_SPACE_POSITIONED
    info = translateToClipSpaceOrigin(info);
    #endif

    #ifdef IS_BILLBOARD
        info = applyBillboarding(info);
    #endif

    vec3 normal = getNormal();

    #ifdef HAS_NORMAL_VEC3
		v_Normal = normalize(vec3(info.normalMatrix * vec4(normal, 0.0)));
		
		#ifdef HAS_TANGENT_VEC4
			vec3 tangentW = normalize(vec3(info.modelMatrix * vec4(getTangent(), 0.0)));
			vec3 bitangentW = cross(v_Normal, tangentW) * a_tangent.w;
			v_TBN = mat3(tangentW, bitangentW, v_Normal);
		#endif // !HAS_TANGENT_VEC4
    #endif // !HAS_NORMAL_VEC3

    vec4 vertexPos = getPosition(); 

    #ifdef IS_FIXEDSIZE
        float yScale = 1400.0 / float(u_viewportResolution.y); // use our "reference" resolution that serves as a basis for fixed size
        float invProjectionScale = (info.viewProjectionMatrix * info.modelMatrix[3]).w;
        vertexPos.xyz *= yScale * u_fixedSizeScale * invProjectionScale * u_orthographicScale;
    #endif// IS_FIXEDSIZE

    vertexPos.xyz += normal * u_ExtrusionFactor;
    vec4 worldPos = info.modelMatrix * vertexPos;
    
    v_Position = vec3(worldPos.xyz) / worldPos.w;

    v_texcoord_0 = vec2(0.0, 0.0);
    v_texcoord_1 = vec2(0.0, 0.0);

    #ifdef HAS_TEXCOORD_0_VEC2
    v_texcoord_0 = a_texcoord_0;
    #endif

    #ifdef HAS_TEXCOORD_1_VEC2
    v_texcoord_1 = a_texcoord_1;
    #endif

    #if defined(HAS_COLOR_0_VEC3) || defined(HAS_COLOR_0_VEC4)
    v_Color = a_color_0;
    #endif

    gl_Position = info.viewProjectionMatrix * worldPos;

    #ifdef IS_SCREEN_SPACE_POSITIONED
    gl_Position = applyScreenSpacePosition(gl_Position);
    #endif //IS_SCREEN_SPACE_POSITIONED
}
