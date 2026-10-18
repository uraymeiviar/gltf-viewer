/**
 * \brief lightmapper.h
 *
 * Based on the OpenGL lightmapping library https://github.com/ands/lightmapper by Andreas Mantler (ands) | last change: 10.05.2018
 *
 * Original license text:
 *
 * This software is in the public domain.
 * Where that dedication is not recognized,
 * you are granted a perpetual, irrevocable license to copy
 * and modify this file however you want.
 */

#pragma once

class QOpenGLFunctions_4_1_Core;

namespace lightmapper {

struct Context;

enum class Mode {
    AmbientOcclusion,
    Thickness
};

enum class Type {
    None,
    UnsignedByte,
    UnsignedShort,
    UnsignedInt,
    Float,
};

/**
 * \brief creates a lightmapper instance. it can be used to render multiple lightmaps.
 *
 * \param hemisphereSize resolution of the hemisphere renderings. must be a power of two! typical: 64.
 * \param interpolationPasses hierarchical selective interpolation passes (0-8; initial step size = 2^passes).
 * \param interpolationThreshold error value below which lightmap pixels are interpolated instead of rendered.
 * the lower the value, the more hemispheres are rendered -> slower, but possibly better quality.
 * \param cameraToSurfaceDistanceModifier modifier for the height of the rendered hemispheres above the surface
 * -1.0f => stick to surface, 0.0f => minimum height for interpolated surface normals
 * > 0.0f => improves gradients on surfaces with interpolated normals due to the flat surface horizon, but may introduce other artifacts.
 */
Context* create(
    QOpenGLFunctions_4_1_Core* gl,
    Mode mode,
    int hemisphereSize,
    float zNear, float zFar,
    float clearR, float clearG, float clearB,
    int interpolationPasses,
    float interpolationThreshold,
    float cameraToSurfaceDistanceModifier = 0.0);

void setHemisphereWeights(Context* ctx);

/**
 * \brief specify an output lightmap image buffer with w * h * c * sizeof(float) bytes of memory.
 */
void setTargetLightmap(Context* ctx, float* outLightmap, int w, int h, int c);

/**
 * \brief set the geometry to map to the currently set target lightmap (set the target lightmap before calling this!).
 *
 * \param transformationMatrix 4x4 object-to-world transform for the geometry or NULL (no transformation).
 * \param normalsType optional normals for the mesh in object space (Use LM_NONE type in case you only need flat surfaces)
 * \param count if mesh indices are used, count = number of indices else count = number of vertices.
 */
void setGeometry(
    Context* ctx,
    const float* transformationMatrix,
    Type positionsType, const void* positionsXYZ, int positionsStride,
    Type normalsType, const void* normalsXYZ, int normalsStride,
    Type texCoordsType, const void* texCoordsUV, int texCoordsStride,
    int count, Type indicesType = Type::None, const void* indices = nullptr);

/**
 * as long as lmBegin returns true, the scene has to be rendered with the
 * returned camera and view parameters to the currently bound framebuffer.
 * if lmBegin returns true, it must be followed by end after rendering!
 *
 * \param output of the current viewport: { x, y, w, h }. use these to call ctx->gl->glViewport()!
 * \param output of the current camera view matrix.
 * \param outProjection4x4 output of the current camera projection matrix.
 */
bool begin(
    Context* ctx,
    int* outViewport4,
    float* outView4x4,
    float* outProjection4x4);

/**
 * provides the light mapping progress as a value increasing from 0.0 to 1.0
 *
 * \warning should only be called between lmBegin/end
 */
float progress(Context* ctx);

void end(Context* ctx);

/**
 * \brief destroys the lightmapper instance. should be called to free resources.
 */
void destroy(Context* ctx);

/**
 * \brief Find maximal value inside image.
 */
float imageMax(const float* image, int width, int height, int channels);

/**
 * \brief widen the populated non-zero areas by 1 pixel.
 */
void imageDilate(const float* image, float* outImage, int width, int height, int channels);

/**
 * \brief casts a floating point image to an 8bit/channel image
 */
void imageFtoUB(const float* image, unsigned char* outImage, int w, int h, int c, float max = 0.0f);

}
