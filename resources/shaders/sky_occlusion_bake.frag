// A3.9: sky-visibility volume bake (docs/ASOBO_EXTENSIONS_PHASE_3.md, A3.9). Two passes share this file
// (header/imgui/SkyOcclusion.h prepends #version and one PASS_* define); both draw one full-screen triangle per slice
// of the volume, so a fragment is one cell (x, y) of slice u_Slice.
//   PASS_ACCUMULATE  tests the cell centre against the batch's orthographic depth maps of the casters and adds
//                    (v, v d) for each direction d (blend ONE, ONE into the RGBA32F accumulator). v is the hardware
//                    depth compare with 2 x 2 bilinear PCF: 1 when the cell lies at least the bias in front of the
//                    outermost surface towards the sky in direction d, so cells on or inside geometry see nothing.
//                    A depth map holds only the outermost surface, which decides a straight ray exactly; a cone
//                    filter (soft per-direction visibility) cannot be estimated from it without bias (a point under
//                    a wall cannot tell an escape past the rim from a crossing of the wall), so each direction stays a
//                    hard test and the lattice's discretisation shows as faint contours (steps of about 1 / N of the
//                    sphere) behind small openings.
//   PASS_FINALIZE    divides by the direction count: (a, m) = (visible fraction, mean visible direction); a cell
//                    without half a visible direction is inside a solid and is marked a = -1 (left out of the lookup's
//                    interpolation, sky_occlusion.glsl).

out vec4 g_Output;

uniform int u_Slice;

#ifdef PASS_ACCUMULATE
const int kBatch = 8;
uniform sampler2DArrayShadow u_DepthMaps;
uniform int u_Count;
uniform vec3 u_Origin;   // world position of the volume's minimum corner
uniform float u_Cell;    // cell edge
uniform mat4 u_TextureFromWorld[kBatch];
uniform vec4 u_DirectionBias[kBatch]; // xyz: unit direction towards the sky; w: bias in texture depth units

void main()
{
    vec3 p = u_Origin + vec3(gl_FragCoord.xy, float(u_Slice) + 0.5) * u_Cell;
    vec4 sum = vec4(0.0);
    for (int i = 0; i < kBatch; ++i) {
        if (i >= u_Count) {
            break;
        }
        vec4 t = u_TextureFromWorld[i] * vec4(p, 1.0);
        float visible = texture(u_DepthMaps, vec4(t.xy, float(i), t.z + u_DirectionBias[i].w));
        sum += visible * vec4(1.0, u_DirectionBias[i].xyz);
    }
    g_Output = sum;
}
#endif

#ifdef PASS_FINALIZE
uniform sampler3D u_Accumulator;
uniform float u_InvDirections;
uniform float u_MinVisible;

void main()
{
    vec4 moments = texelFetch(u_Accumulator, ivec3(ivec2(gl_FragCoord.xy), u_Slice), 0) * u_InvDirections;
    g_Output = moments.x < u_MinVisible ? vec4(-1.0, 0.0, 0.0, 0.0) : moments;
}
#endif
