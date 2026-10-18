uniform float u_maxDistance = 40.0f;
uniform float u_blendDistance = 20.0f; // Describes the distance from u_maxDistance where blending is applied
uniform vec4 u_color = vec4(0.72, 0.72, 0.72, 1.0);

layout(location = 0) out vec4 g_fragColor;

void main()
{
	float dist = gl_FragCoord.z / gl_FragCoord.w;
	if(dist > u_maxDistance)
	{
		discard;
	}

	//Alpha is 0 at u_maxDistance and 1 at u_maxDistance - u_blendDistance
	g_fragColor = vec4(u_color.rgb, u_color.a * clamp((u_maxDistance - dist) / u_blendDistance, 0, 1));
}
