uniform vec3 u_WireframeColor = vec3(1.0);
layout(location = 0) out vec4 g_fragColor;

void main()
{	
	g_fragColor = vec4(u_WireframeColor,1);
}
