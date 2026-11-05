// The UV grid overlay in every view but the two texture views, which have it in their surface: drawn over the finished
// image like the wireframe overlay, premultiplied. docs/TEXTURE_METRICS.md.
layout(location = 0) out vec4 g_fragColor;

#include <uv_grid_inputs.glsl>
#include <uv_grid.glsl>

void main()
{
    if (u_UvHeatmapUvSet < 0) {
        discard; // no such UV set on this primitive
    }
    vec4 layer = uvGridLayer();
    if (layer.a <= 0.0) {
        discard; // nothing here: no depth either, so what is drawn after it is not affected
    }
    g_fragColor = layer;
}
