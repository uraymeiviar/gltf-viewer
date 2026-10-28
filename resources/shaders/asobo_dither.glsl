// A1.7: ASOBO_material_alphamode_dither (docs/ASOBO_EXTENSIONS_SEMANTICS.md section 6). Screen-door
// transparency in the opaque pass: a fragment is discarded where its alpha does not exceed an ordered
// 4x4 Bayer threshold, so a 4x4 block keeps round(16 * alpha) pixels (exact at multiples of 1/16) and
// the kept pixels are fully opaque, write depth and need no sorting. The SDK only says "blended with
// the background using a dithering pattern"; the matrix and the per-frame offset are Glider's choice.
// The forward pass, the decal prepass and picking all call asoboDitherDiscard, so they keep the same
// pixels. The material define is asset-level; whether the viewport applies it is the frame-block flag
// u_AsoboAlphaDitherApplied, so the command cache does not depend on the viewport.
#ifdef ASOBO_ALPHA_DITHER

// Picking draws one pixel into a 1x1 target; this is that pixel's position in the viewport so the
// threshold matches the forward pass. 0 everywhere else.
uniform vec2 u_AsoboDitherPixelOrigin;

// Threshold in (0, 1) for a viewport pixel. Without TAA (u_AsoboDitherFrame == 0) it is the fixed
// Bayer value (index + 0.5) / 16. With TAA every frame adds frame * (golden ratio - 1) modulo 1: each
// 4x4 block still holds 16 evenly spaced thresholds (same coverage per frame), while every pixel walks
// a low-discrepancy sequence over time, so the TAA history converges to the alpha-weighted average.
float asoboDitherThreshold(vec2 fragCoord)
{
    const float bayer[16] = float[16](0.0, 8.0, 2.0, 10.0,
                                      12.0, 4.0, 14.0, 6.0,
                                      3.0, 11.0, 1.0, 9.0,
                                      15.0, 7.0, 13.0, 5.0);
    ivec2 cell = ivec2(floor(fragCoord + u_AsoboDitherPixelOrigin)) & ivec2(3);
    float threshold = (bayer[cell.y * 4 + cell.x] + 0.5) / 16.0;
    return fract(threshold + u_AsoboDitherFrame * 0.6180339887);
}

// True when the viewport applies the extension and this pixel is dithered away. Alpha 0 discards
// every pixel; alpha 1 keeps every pixel (the largest threshold is below 1).
bool asoboDitherDiscard(float alpha)
{
    return u_AsoboAlphaDitherApplied != 0 && alpha <= asoboDitherThreshold(gl_FragCoord.xy);
}

#endif
