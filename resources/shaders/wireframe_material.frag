uniform vec4 u_WireframeColor = vec4(1.0);
layout(location = 0) out vec4 g_fragColor;

void main()
{	
	g_fragColor = u_WireframeColor;
}
