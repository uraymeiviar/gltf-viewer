#include <camera_block.glsl>
#include <environment_block.glsl>


in vec3 a_position;
out vec3 v_TexCoords;


void main()
{
    v_TexCoords = u_EnvRotation * a_position;
    mat4 mat = u_ViewProjectionMatrix;
    mat[3] = vec4(0.0, 0.0, 0.0, 0.1);
    vec4 pos = mat * vec4(a_position, 1.0);
    gl_Position = pos.xyww;
}
