// A3.6: ground plane (docs/ASOBO_EXTENSIONS_PHASE_3.md). A screen-covering quad; the fragment shader
// intersects each pixel's view ray with the horizontal plane (the ray-grid path of GridRenderer). The two
// varyings are the pixel's points on the near and far plane in world space, from the camera block's
// (possibly TAA-jittered) view-projection matrix, so the plane's depth matches scene geometry exactly.
#include <camera_block.glsl>


in vec3 a_position;
out vec3 v_NearPoint;
out vec3 v_FarPoint;


void main()
{
    mat4 inverseViewProjection = inverse(u_ViewProjectionMatrix);
    vec4 nearPoint = inverseViewProjection * vec4(a_position.xy, -1.0, 1.0);
    vec4 farPoint = inverseViewProjection * vec4(a_position.xy, 1.0, 1.0);
    v_NearPoint = nearPoint.xyz / nearPoint.w;
    v_FarPoint = farPoint.xyz / farPoint.w;
    gl_Position = vec4(a_position.xy, 0.0, 1.0);
}
