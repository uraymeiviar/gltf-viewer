// An extremly simple shader used for editor helper geometry.

uniform mat4 u_ModelMatrix;
uniform mat4 u_ViewProjectionMatrix;

in vec3 a_position;

void main()
{
    gl_Position = u_ViewProjectionMatrix * u_ModelMatrix * vec4(a_position, 1);
}
