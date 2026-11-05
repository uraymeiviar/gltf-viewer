// The UV grid and cell labels: in the Texture Distortion / Density views (pbr.frag) and as an overlay in every other view
// (uv_grid.frag). Reads v_texcoord_0 and v_texcoord_1. docs/TEXTURE_METRICS.md.
// ShaderCache puts every include ahead of the file's own text, whatever #if surrounds it: pbr.frag programs of other
// views carry none of this because of the guard here.
#if defined(UV_HEATMAP) || defined(UV_GRID_OVERLAY_FRAG)
uniform vec4 u_UvHeatmapView; // x heat opacity, y grid cells (0: no grid), z cell labels, w grid opacity
uniform int u_UvHeatmapUvSet; // the UV set the table measured; below 0: not one the renderer carries (no grid)
uniform vec3 u_UvHeatmapUvX; // that UV's transforms as one affine map: u' = dot(x, vec3(u, v, 1))
uniform vec3 u_UvHeatmapUvY; // v' = dot(y, vec3(u, v, 1))

// The labels' characters, 0-9 then A-Z: seven rows of five bits, top row first.
const int kUvHeatmapFont[252] = int[252](
    0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E,
    0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E,
    0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F,
    0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E,
    0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02,
    0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E,
    0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E,
    0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08,
    0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E,
    0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C,
    0x0E, 0x11, 0x11, 0x11, 0x1F, 0x11, 0x11,
    0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E,
    0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E,
    0x1C, 0x12, 0x11, 0x11, 0x11, 0x12, 0x1C,
    0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F,
    0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10,
    0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F,
    0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11,
    0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E,
    0x07, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0C,
    0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11,
    0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F,
    0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11,
    0x11, 0x11, 0x19, 0x15, 0x13, 0x11, 0x11,
    0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E,
    0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10,
    0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D,
    0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11,
    0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E,
    0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
    0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E,
    0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04,
    0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A,
    0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11,
    0x11, 0x11, 0x11, 0x0A, 0x04, 0x04, 0x04,
    0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F);

// The label of a cell, as the add-on's glyph_of: column letters (A..Z, AA..) then the row number from 1; -1 past its end.
int uvHeatmapGlyph(int slot, int col, int row)
{
    int letters = col < 26 ? 1 : 2;
    int number = row + 1;
    int digits = number < 10 ? 1 : 2;
    if (slot < letters) {
        if (letters == 1) {
            return 10 + col;
        }
        return slot == 0 ? 10 + (col / 26 - 1) : 10 + (col - (col / 26) * 26);
    }
    int d = slot - letters;
    if (d >= digits) {
        return -1;
    }
    if (digits == 1) {
        return number;
    }
    return d == 0 ? number / 10 : number - (number / 10) * 10;
}

// One bit of a glyph; p in font pixels within the slot of 6 x 8 (one column and half a row each side stay empty).
float uvHeatmapFontBit(int glyph, vec2 p)
{
    int x = int(floor(p.x));
    int y = int(floor(p.y - 0.5));
    if (x < 0 || x > 4 || y < 0 || y > 6) {
        return 0.0;
    }
    return float((kUvHeatmapFont[glyph * 7 + y] >> (4 - x)) & 1);
}

vec4 uvGridOver(vec4 layer, vec3 colour, float alpha)
{
    return vec4(colour * alpha, alpha) + layer * (1.0 - alpha);
}

// The add-on's UV grid as a premultiplied layer: what to put over the surface. Its UVs are Blender's (V up, from the image's lower left), so a face has
// the same cell label in both tools. Glider's own colours, so nothing is mistaken for something else on a bent surface:
// lines of constant U are violet, lines of constant V orange, hues the heat scale does not use, and the labels take one
// of four other dark colours, a different one from all eight neighbouring cells (the legend says so).
vec4 uvGridLayer()
{
    vec4 layer = vec4(0.0);
    vec3 uv = vec3(u_UvHeatmapUvSet < 1 ? v_texcoord_0 : v_texcoord_1, 1.0);
    float cells = u_UvHeatmapView.y;
    vec2 g = vec2(dot(u_UvHeatmapUvX, uv), 1.0 - dot(u_UvHeatmapUvY, uv)) * cells;
    vec2 cell = floor(g);
    vec2 f = g - cell;
    vec2 gx = dFdx(g);
    vec2 gy = dFdy(g);
    vec2 w = max(abs(gx) + abs(gy), vec2(1e-6));
    vec2 d = min(f, 1.0 - f) / w; // distance to the nearest cell border, in pixels
    vec2 line = 1.0 - clamp(d - 0.5, 0.0, 1.0);
    layer = uvGridOver(layer, vec3(0.32, 0.00, 0.60), 0.85 * line.x * u_UvHeatmapView.w);
    layer = uvGridOver(layer, vec3(0.90, 0.42, 0.00), 0.85 * line.y * u_UvHeatmapView.w);
    float cellPixels = 1.0 / max(w.x, w.y); // a cell's size on screen
    if (u_UvHeatmapView.z > 0.5 && cellPixels > 22.0) { // labels fade in from 22 to 34 pixel cells
        const float gh = 0.26; // glyph height as a share of the cell
        const float gw = gh * 0.62;
        vec2 q = (f - vec2(0.07, 1.0 - 0.07 - gh)) / vec2(gw, gh);
        if (q.y >= 0.0 && q.y < 1.0 && q.x >= 0.0 && q.x < 4.0) {
            int slot = int(floor(q.x));
            int col = int(mod(cell.x, cells));
            int row = int(mod(cell.y, cells));
            int glyph = uvHeatmapGlyph(slot, col, row);
            if (glyph >= 0) {
                const vec2 fontPixels = vec2(6.0, 8.0);
                vec2 p = vec2(fract(q.x), 1.0 - q.y) * fontPixels;
                // Taps in the pixel's footprint, with the strokes' gain of the add-on, so thin strokes do not fade small.
                vec2 dx = gx * vec2(1.0, -1.0) / vec2(gw, gh) * fontPixels * 0.35;
                vec2 dy = gy * vec2(1.0, -1.0) / vec2(gw, gh) * fontPixels * 0.35;
                float a = 0.25 * (uvHeatmapFontBit(glyph, p + dx) + uvHeatmapFontBit(glyph, p - dx)
                                  + uvHeatmapFontBit(glyph, p + dy) + uvHeatmapFontBit(glyph, p - dy));
                a = smoothstep(0.15, 0.55, a);
                float fade = clamp((cellPixels - 22.0) / 12.0, 0.0, 1.0);
                const vec3 inks[4] = vec3[4](vec3(0.03, 0.03, 0.03), vec3(0.00, 0.30, 0.32), vec3(0.34, 0.22, 0.00), vec3(0.50, 0.00, 0.24));
                layer = uvGridOver(layer, inks[(col + 2 * row) % 4], 0.9 * a * fade * u_UvHeatmapView.w);
            }
        }
    }
    return layer;
}
#endif
