
#extension GL_ARB_texture_multisample : enable

in vec4 v_Position;
in vec2 v_outUV;

out vec4 g_finalColor;

uniform sampler2DMS u_texSampler;
uniform int u_samples = 1;

void main()
{
    g_finalColor = vec4( 0,0,0,1);

    ivec2 coord = ivec2(gl_FragCoord.xy - vec2(0.5f)); // textureSize(u_texSampler)

    for(int i = 0; i < u_samples; ++i)
    {
        g_finalColor.rgb += texelFetch(u_texSampler, coord, i).rgb; // v_outUV
    }

     g_finalColor.rgb *= 1.f / float(u_samples);
}