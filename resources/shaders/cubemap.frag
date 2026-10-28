precision highp float;


#include <environment_block.glsl>
#include <tonemapping.glsl>


uniform samplerCube u_GGXEnvSampler;

#ifdef PROCEDURAL_SKY
#include <sky_common.glsl>
// A6.2: the procedural sky, drawn from the sky-view LUT every frame (so it follows the sun at once, independent of
// the time-sliced IBL cubes). v_TexCoords is the world direction: the environment rotation is identity in this mode.
uniform sampler2D u_SkyViewLUT;
uniform vec3 u_SkySunDirection; // world, unit, towards the sun
uniform vec4 u_SkyParams;       // x bottom radius, y top radius, z observer radius (km), w radiance scale
#endif

out vec4 FragColor;
in vec3 v_TexCoords;


void main()
{
#ifdef PROCEDURAL_SKY
    vec3 direction = normalize(v_TexCoords);
    vec3 sky = skyViewRadiance(u_SkyViewLUT, direction, normalize(u_SkySunDirection), u_SkyParams.x, u_SkyParams.z) * u_SkyParams.w;
    // A6.7 hook: add the limb-darkened sun disk here, to the background only (the IBL filters never see it), with
    // the A3.1 sun colour (atmosphere transmittance) and u_SunAngularRadius.
    // A6.6 hook: the moon disk and the stars (rotated by sidereal time) are added here too, attenuated by the
    // transmittance towards them; their contribution to the IBL goes through sky_passes.frag PASS_CUBE.
    vec4 color = vec4(sky, 1.0);
#else
    vec4 color = textureLod(u_GGXEnvSampler, v_TexCoords, u_EnvBlurNormalized * float(u_MipCount - 1)) * u_EnvIntensity;
#endif

#ifdef LINEAR_OUTPUT
    FragColor = color.rgba;
#else
    FragColor = vec4(toneMap(color.rgb), color.a);
#endif
}
