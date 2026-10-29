#include <camera_block.glsl>
#include <environment_block.glsl>


in vec3 a_position;
out vec3 v_TexCoords;


void main()
{
    v_TexCoords = u_EnvRotation * a_position;
    mat4 mat = u_ViewProjectionMatrix;
    // Rotation only: the cube is centred on the camera. A perspective projection gets no clip-w offset, so every
    // background pixel shows the world direction that projects to it (pos.xyww keeps it at the far plane). The
    // orthographic path keeps the original w = 0.1 (its w would otherwise be zero); cubemap.frag trueWorldDirection
    // pairs with it.
    mat[3] = vec4(0.0, 0.0, 0.0, u_ProjectionMatrix[3][3] == 0.0 ? 0.0 : 0.1);
    vec4 pos = mat * vec4(a_position, 1.0);
    gl_Position = pos.xyww;
}
