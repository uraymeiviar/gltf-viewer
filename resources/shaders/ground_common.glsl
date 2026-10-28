// A3.6: constants, helpers and environment samplers ground.frag needs before brdf.glsl, ibl.glsl and shadow.glsl.

const float M_PI = 3.141592653589793;

float clampedDot(vec3 x, vec3 y)
{
    return clamp(dot(x, y), 0.0, 1.0);
}

vec3 fitHalfFloatHdr(vec3 color)
{
    float peak = max(max(color.r, color.g), color.b);
    return peak > 13000.0 ? color * (13000.0 / peak) : color;
}

// Declared always (ibl.glsl needs them); they are active only where the shader uses them (GROUND_ENV), so
// a draw without an environment binds none.
uniform samplerCube u_LambertianEnvSampler;
uniform samplerCube u_GGXEnvSampler;
uniform samplerCube u_CharlieEnvSampler;
uniform sampler2D u_GGXLUT;
uniform sampler2D u_CharlieLUT;

