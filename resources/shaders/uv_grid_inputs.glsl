// What uv_grid.glsl needs in uv_grid.frag; pbr.frag has the inputs from textures.glsl. Included first (ShaderCache keeps the
// includes' order, ahead of the file's own text).
#define UV_GRID_OVERLAY_FRAG
in vec2 v_texcoord_0;
in vec2 v_texcoord_1;
