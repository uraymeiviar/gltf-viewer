// A6.8: the ocean surface (docs/ASOBO_EXTENSIONS_PHASE_6.md, A6.8). A camera-centred polar grid (GltfResourceManager
// initOcean: a_position.x = angle / 2 pi, a_position.y = ring parameter v in [0, 1]) with radius s (e^(v L) - 1): rings
// grow in proportion to their distance, so the triangles keep about the same size on screen out to the horizon
// (r_max = s (e^L - 1)). It is centred on the camera in X/Z (no rotation with the view, so turning the camera never
// moves a vertex), and every vertex is displaced by the ocean cascades sampled at the mip level of its ring spacing
// (the far rings take the band's mean, so the geometry does not alias). Vertices beyond the far plane are kept (their
// clip z is limited to the far plane); ocean.frag writes each fragment's true depth (1 beyond the far plane).
#include <camera_block.glsl>

in vec3 a_position;

uniform sampler2DArray u_OceanDisplacement;
uniform vec4 u_OceanGrid;     // x: sea level (world Y), y: ring scale s, z: L = ln(1 + r_max / s), w: rings - 1
uniform vec4 u_OceanInfo;     // x: cascade count, y: texels per side N, z: foam threshold, w: unresolved slope variance
// Per cascade: x: 1 / patch size, y: layer of the frame before the current time, z: layer of the frame after, w: blend
uniform vec4 u_OceanCascade0;
uniform vec4 u_OceanCascade1;
uniform vec4 u_OceanCascade2;
uniform vec4 u_OceanCascade3;

out vec3 v_WorldPosition;
out vec2 v_SurfaceCoord; // the undisplaced X/Z, where the cascades are sampled
out float v_Height;      // the displaced height above sea level

const float OCEAN_PI = 3.141592653589793;

vec3 oceanCascadeDisplacement(vec4 cascade, vec2 xz, float spacing)
{
    float lod = max(log2(spacing * cascade.x * u_OceanInfo.y), 0.0);
    vec2 uv = xz * cascade.x;
    vec3 a = textureLod(u_OceanDisplacement, vec3(uv, cascade.y), lod).xyz;
    vec3 b = textureLod(u_OceanDisplacement, vec3(uv, cascade.z), lod).xyz;
    return mix(a, b, cascade.w);
}

void main()
{
    float s = u_OceanGrid.y;
    float r = s * (exp(a_position.y * u_OceanGrid.z) - 1.0);
    float angle = a_position.x * 2.0 * OCEAN_PI;
    vec2 xz = u_Camera.xz + r * vec2(cos(angle), sin(angle));
    // Distance between neighbouring rings here (dr / dv / rings).
    float spacing = max((r + s) * u_OceanGrid.z / max(u_OceanGrid.w, 1.0), 1.0e-4);

    int count = int(u_OceanInfo.x + 0.5);
    vec3 d = vec3(0.0);
    if (count > 0) d += oceanCascadeDisplacement(u_OceanCascade0, xz, spacing);
    if (count > 1) d += oceanCascadeDisplacement(u_OceanCascade1, xz, spacing);
    if (count > 2) d += oceanCascadeDisplacement(u_OceanCascade2, xz, spacing);
    if (count > 3) d += oceanCascadeDisplacement(u_OceanCascade3, xz, spacing);

    vec3 world = vec3(xz.x + d.x, u_OceanGrid.x + d.y, xz.y + d.z);
    v_WorldPosition = world;
    v_SurfaceCoord = xz;
    v_Height = d.y;
    vec4 clip = u_ViewProjectionMatrix * vec4(world, 1.0);
    if (clip.w > 0.0 && clip.z > clip.w) {
        clip.z = clip.w;
    }
    gl_Position = clip;
}
