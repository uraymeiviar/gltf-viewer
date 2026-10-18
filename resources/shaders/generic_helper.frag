// An extremly simple shader used for editor helper geometry.

uniform vec3 u_Color = vec3(1);

out vec4 f_Color;

void main()
{	
	f_Color = vec4(u_Color, 1);
}
