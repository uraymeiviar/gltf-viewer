// A2.1: fullscreen view of the decal prepass targets for the "Decal prepass depth" and
// "Decal prepass normal" Render Modes.

in vec2 v_outUV;

uniform sampler2D u_PreviewDepthSampler;
uniform sampler2D u_PreviewNormalSampler;
uniform int u_PreviewMode;
// The shared buffer may be larger than this viewport; sample only the rendered sub-rect.
uniform vec2 u_PreviewUVScale;

layout(location = 0) out vec4 g_finalColor;

void main()
{
    vec2 uv = v_outUV * u_PreviewUVScale;
    if (u_PreviewMode == 0)
    {
        // Raw normalized window depth, inverted and contrast-stretched: cockpit-scale
        // geometry sits close to 1.0 on a 0.1-1000 clip range, so the plain value is white.
        float depth = clamp(texture(u_PreviewDepthSampler, uv).r, 0.0, 1.0);
        g_finalColor = vec4(vec3(1.0 - pow(depth, 32.0)), 1.0);
    }
    else
    {
        // G0 already stores the world-space normal as n * 0.5 + 0.5.
        g_finalColor = vec4(texture(u_PreviewNormalSampler, uv).rgb, 1.0);
    }
}
