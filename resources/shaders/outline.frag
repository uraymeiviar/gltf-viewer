in vec4 v_position;
in vec2 v_outUV;

out vec4 g_finalColor;

uniform vec4 u_outlineColor = vec4(0);
uniform int u_outlineThickness = 5;
uniform sampler2D u_framebuffer;

void main()
{
    ivec2 centerCoord = ivec2(gl_FragCoord.xy); 

    // Do not draw over content.
    if (texelFetch(u_framebuffer, centerCoord, 0).a >= 1.0) {
        discard;
    }

    // Sample a disc around the current pixel to estimate local content coverage.
    float acc = 0.0;
    for (int iy = -u_outlineThickness; iy <= u_outlineThickness; ++iy) {
        for (int ix = -u_outlineThickness; ix <= u_outlineThickness; ++ix) {
            if (ix * ix + iy * iy > u_outlineThickness * u_outlineThickness) continue;
            acc += texelFetch(u_framebuffer, centerCoord + ivec2(ix, iy), 0).a;
        }
    }
    g_finalColor = vec4(u_outlineColor.rgb, step(0.5, acc));
}
