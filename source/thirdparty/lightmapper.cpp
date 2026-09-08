#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>

#include <glm.hpp>

#include "thirdparty/lightmapper.h"

#include <QOpenGLFunctions_4_1_Core>

namespace lightmapper {
struct Context {
    struct
    {
        const float* modelMatrix;
        float normalMatrix[9];

        const unsigned char* positions;
        Type positionsType;
        int positionsStride;
        const unsigned char* normals;
        Type normalsType;
        int normalsStride;
        const unsigned char* uvs;
        Type uvsType;
        int uvsStride;
        const unsigned char* indices;
        Type indicesType;
        unsigned int count;
    } mesh;

    struct
    {
        int pass;
        int passCount;

        struct
        {
            unsigned int baseIndex;
            glm::vec3 p[3];
            glm::vec3 n[3];
            glm::vec2 uv[3];
        } triangle;

        struct
        {
            int minx, miny;
            int maxx, maxy;
            int x, y;
        } rasterizer;

        struct
        {
            glm::vec3 position;
            glm::vec3 direction;
            glm::vec3 up;
        } sample;

        struct
        {
            int side;
        } hemisphere;
    } meshPosition;

    struct
    {
        int width;
        int height;
        int channels;
        float* data;
    } lightmap;

    struct
    {
        unsigned int size;
        float zNear, zFar;
        float cameraToSurfaceDistanceModifier;
        struct {
            float r, g, b;
        } clearColor;

        unsigned int fbHemiCountX;
        unsigned int fbHemiCountY;
        unsigned int fbHemiIndex;
        glm::ivec2* fbHemiToLightmapLocation;
        GLuint fbTexture[2];
        GLuint fb[2];
        GLuint fbDepth;
        GLuint vao;
        struct
        {
            GLuint programID;
            GLuint hemispheresTextureID;
            GLuint weightsTextureID;
            GLuint weightsTexture;
        } firstPass;
        struct
        {
            GLuint programID;
            GLuint hemispheresTextureID;
        } downsamplePass;
        struct
        {
            GLuint texture;
            glm::ivec2 writePosition;
            int width, height;
            glm::ivec2* toLightmapLocation;
        } storage;
    } hemisphere;

    float interpolationThreshold;

    Mode mode = Mode::AmbientOcclusion;
    QOpenGLFunctions_4_1_Core* gl = nullptr;
};
}

namespace {

glm::vec2 toBarycentric(glm::vec2 p1, glm::vec2 p2, glm::vec2 p3, glm::vec2 p)
{
    // http://www.blackpawn.com/texts/pointinpoly/
    // Compute vectors
    glm::vec2 v0 = p3 - p1;
    glm::vec2 v1 = p2 - p1;
    glm::vec2 v2 = p - p1;
    // Compute dot products
    float dot00 = glm::dot(v0, v0);
    float dot01 = glm::dot(v0, v1);
    float dot02 = glm::dot(v0, v2);
    float dot11 = glm::dot(v1, v1);
    float dot12 = glm::dot(v1, v2);
    // Compute barycentric coordinates
    float invDenom = 1.0f / (dot00 * dot11 - dot01 * dot01);
    float u = (dot11 * dot02 - dot01 * dot12) * invDenom;
    float v = (dot00 * dot12 - dot01 * dot02) * invDenom;
    return glm::vec2(u, v);
}

// pseudo cross product
float cross2(glm::vec2 a, glm::vec2 b)
{
    return a.x * b.y - a.y * b.x;
}

// positive mod
float pmodf(float a, float b)
{
    return (a < 0.0f ? 1.0f : 0.0f) + (float)fmod(a, b);
}

int leftOf(glm::vec2 a, glm::vec2 b, glm::vec2 c)
{
    float x = cross2(b - a, c - b);
    return x < 0 ? -1 : x > 0;
}

bool lineIntersection(glm::vec2 x0, glm::vec2 x1, glm::vec2 y0, glm::vec2 y1, glm::vec2* res)
{
    glm::vec2 dx = x1 - x0;
    glm::vec2 dy = y1 - y0;
    glm::vec2 d = x0 - y0;
    float dyx = cross2(dy, dx);
    if (dyx == 0.0f)
        return false;
    dyx = cross2(d, dx) / dyx;
    if (dyx <= 0 || dyx >= 1)
        return false;
    res->x = y0.x + dyx * dy.x;
    res->y = y0.y + dyx * dy.y;
    return true;
}

// this modifies the poly array! the poly array must be big enough to hold the result!
// res must be big enough to hold the result!
int convexClip(glm::vec2* poly, int nPoly, const glm::vec2* clip, int nClip, glm::vec2* res)
{
    int nRes = nPoly;
    int dir = leftOf(clip[0], clip[1], clip[2]);
    for (int i = 0, j = nClip - 1; i < nClip && nRes; j = i++) {
        if (i != 0)
            for (nPoly = 0; nPoly < nRes; nPoly++)
                poly[nPoly] = res[nPoly];
        nRes = 0;
        glm::vec2 v0 = poly[nPoly - 1];
        int side0 = leftOf(clip[j], clip[i], v0);
        if (side0 != -dir)
            res[nRes++] = v0;
        for (int k = 0; k < nPoly; k++) {
            glm::vec2 v1 = poly[k], x;
            int side1 = leftOf(clip[j], clip[i], v1);
            if (side0 + side1 == 0 && side0 && lineIntersection(clip[j], clip[i], v0, v1, &x))
                res[nRes++] = x;
            if (k == nPoly - 1)
                break;
            if (side1 != -dir)
                res[nRes++] = v1;
            v0 = v1;
            side0 = side1;
        }
    }

    return nRes;
}

// pass order of one 4x4 interpolation patch for two interpolation steps (and the next neighbors right of/below it)
// 0 4 1 4 0
// 5 6 5 6 5
// 2 4 3 4 2
// 5 6 5 6 5
// 0 4 1 4 0

unsigned int passStepSize(lightmapper::Context* ctx)
{
    unsigned int shift = ctx->meshPosition.passCount / 3 - (ctx->meshPosition.pass - 1) / 3;
    unsigned int step = (1 << shift);
    assert(step > 0);
    return step;
}

unsigned int passOffsetX(lightmapper::Context* ctx)
{
    if (!ctx->meshPosition.pass)
        return 0;
    int passType = (ctx->meshPosition.pass - 1) % 3;
    unsigned int halfStep = passStepSize(ctx) >> 1;
    return passType != 1 ? halfStep : 0;
}

unsigned int passOffsetY(lightmapper::Context* ctx)
{
    if (!ctx->meshPosition.pass)
        return 0;
    int passType = (ctx->meshPosition.pass - 1) % 3;
    unsigned int halfStep = passStepSize(ctx) >> 1;
    return passType != 0 ? halfStep : 0;
}

bool hasConservativeTriangleRasterizerFinished(lightmapper::Context* ctx)
{
    return ctx->meshPosition.rasterizer.y >= ctx->meshPosition.rasterizer.maxy;
}

void moveToNextPotentialConservativeTriangleRasterizerPosition(lightmapper::Context* ctx)
{
    unsigned int step = passStepSize(ctx);
    ctx->meshPosition.rasterizer.x += step;
    while (ctx->meshPosition.rasterizer.x >= ctx->meshPosition.rasterizer.maxx) {
        ctx->meshPosition.rasterizer.x = ctx->meshPosition.rasterizer.minx + passOffsetX(ctx);
        ctx->meshPosition.rasterizer.y += step;
        if (hasConservativeTriangleRasterizerFinished(ctx))
            break;
    }
}

float* getLightmapPixel(lightmapper::Context* ctx, int x, int y)
{
    assert(x >= 0 && x < ctx->lightmap.width && y >= 0 && y < ctx->lightmap.height);
    return ctx->lightmap.data + (y * ctx->lightmap.width + x) * ctx->lightmap.channels;
}

void setLightmapPixel(lightmapper::Context* ctx, int x, int y, float* in)
{
    assert(x >= 0 && x < ctx->lightmap.width && y >= 0 && y < ctx->lightmap.height);
    float* p = ctx->lightmap.data + (y * ctx->lightmap.width + x) * ctx->lightmap.channels;
    for (int j = 0; j < ctx->lightmap.channels; j++)
        *p++ = *in++;
}

const constexpr float baseAngle = 0.1f;
const float baseAngles[3][3] = {
    { baseAngle, baseAngle + 1.0f / 3.0f, baseAngle + 2.0f / 3.0f },
    { baseAngle + 1.0f / 3.0f, baseAngle + 2.0f / 3.0f, baseAngle },
    { baseAngle + 2.0f / 3.0f, baseAngle, baseAngle + 1.0f / 3.0f }
};

bool trySamplingConservativeTriangleRasterizerPosition(lightmapper::Context* ctx)
{
    if (hasConservativeTriangleRasterizerFinished(ctx))
        return false;

    // check if lightmap pixel was already set
    float* pixelValue = getLightmapPixel(ctx, ctx->meshPosition.rasterizer.x, ctx->meshPosition.rasterizer.y);
    for (int j = 0; j < ctx->lightmap.channels; j++)
        if (pixelValue[j] != 0.0f)
            return false;

    // try calculating centroid by clipping the pixel against the triangle
    glm::vec2 pixel[16];
    pixel[0] = glm::ivec2(ctx->meshPosition.rasterizer.x, ctx->meshPosition.rasterizer.y);
    pixel[1] = glm::ivec2(ctx->meshPosition.rasterizer.x + 1, ctx->meshPosition.rasterizer.y);
    pixel[2] = glm::ivec2(ctx->meshPosition.rasterizer.x + 1, ctx->meshPosition.rasterizer.y + 1);
    pixel[3] = glm::ivec2(ctx->meshPosition.rasterizer.x, ctx->meshPosition.rasterizer.y + 1);

    glm::vec2 res[16];
    int nRes = convexClip(pixel, 4, ctx->meshPosition.triangle.uv, 3, res);
    if (nRes == 0)
        return false; // nothing left

    // calculate centroid position and area
    glm::vec2 centroid = res[0];
    float area = res[nRes - 1].x * res[0].y - res[nRes - 1].y * res[0].x;
    for (int i = 1; i < nRes; i++) {
        centroid = centroid + res[i];
        area += res[i - 1].x * res[i].y - res[i - 1].y * res[i].x;
    }
    centroid /= (float)nRes;
    area = std::abs(area / 2.0f);

    if (area <= 0.0f)
        return false; // no area left

    // calculate barycentric coords
    glm::vec2 uv = toBarycentric(
        ctx->meshPosition.triangle.uv[0],
        ctx->meshPosition.triangle.uv[1],
        ctx->meshPosition.triangle.uv[2],
        centroid);

    if (glm::any(glm::isinf(uv)))
        return false; // degenerate

    // try to interpolate color from neighbors:
    if (ctx->meshPosition.pass > 0) {
        float* neighbors[4];
        int neighborCount = 0;
        int neighborsExpected = 0;
        int d = (int)passStepSize(ctx) / 2;
        int dirs = ((ctx->meshPosition.pass - 1) % 3) + 1;
        if (dirs & 1) // check x-neighbors with distance d
        {
            neighborsExpected += 2;
            if (ctx->meshPosition.rasterizer.x - d >= ctx->meshPosition.rasterizer.minx && ctx->meshPosition.rasterizer.x + d <= ctx->meshPosition.rasterizer.maxx) {
                neighbors[neighborCount++] = getLightmapPixel(ctx, ctx->meshPosition.rasterizer.x - d, ctx->meshPosition.rasterizer.y);
                neighbors[neighborCount++] = getLightmapPixel(ctx, ctx->meshPosition.rasterizer.x + d, ctx->meshPosition.rasterizer.y);
            }
        }
        if (dirs & 2) // check y-neighbors with distance d
        {
            neighborsExpected += 2;
            if (ctx->meshPosition.rasterizer.y - d >= ctx->meshPosition.rasterizer.miny && ctx->meshPosition.rasterizer.y + d <= ctx->meshPosition.rasterizer.maxy) {
                neighbors[neighborCount++] = getLightmapPixel(ctx, ctx->meshPosition.rasterizer.x, ctx->meshPosition.rasterizer.y - d);
                neighbors[neighborCount++] = getLightmapPixel(ctx, ctx->meshPosition.rasterizer.x, ctx->meshPosition.rasterizer.y + d);
            }
        }
        if (neighborCount == neighborsExpected) // are all interpolation neighbors available?
        {
            // calculate average neighbor pixel value
            float avg[4] = { 0 };
            for (int i = 0; i < neighborCount; i++)
                for (int j = 0; j < ctx->lightmap.channels; j++)
                    avg[j] += neighbors[i][j];
            float ni = 1.0f / neighborCount;
            for (int j = 0; j < ctx->lightmap.channels; j++)
                avg[j] *= ni;

            // check if error from average pixel to neighbors is above the interpolation threshold
            bool interpolate = true;
            for (int i = 0; i < neighborCount; i++) {
                bool zero = true;
                for (int j = 0; j < ctx->lightmap.channels; j++) {
                    if (neighbors[i][j] != 0.0f)
                        zero = false;
                    if (fabs(neighbors[i][j] - avg[j]) > ctx->interpolationThreshold)
                        interpolate = false;
                }
                if (zero)
                    interpolate = false;
                if (!interpolate)
                    break;
            }

            // set interpolated value and return if interpolation is acceptable
            if (interpolate) {
                setLightmapPixel(ctx, ctx->meshPosition.rasterizer.x, ctx->meshPosition.rasterizer.y, avg);
                return false;
            }
        }
    }

    // could not interpolate. must render a hemisphere.
    // calculate 3D sample position and orientation
    glm::vec3 p0 = ctx->meshPosition.triangle.p[0];
    glm::vec3 p1 = ctx->meshPosition.triangle.p[1];
    glm::vec3 p2 = ctx->meshPosition.triangle.p[2];
    glm::vec3 v1 = p1 - p0;
    glm::vec3 v2 = p2 - p0;
    ctx->meshPosition.sample.position = p0 + uv.x * v2 + uv.y * v1;

    glm::vec3 n0 = ctx->meshPosition.triangle.n[0];
    glm::vec3 n1 = ctx->meshPosition.triangle.n[1];
    glm::vec3 n2 = ctx->meshPosition.triangle.n[2];
    glm::vec3 nv1 = n1 - n0;
    glm::vec3 nv2 = n2 - n0;
    ctx->meshPosition.sample.direction = glm::normalize(n0 + uv.x * nv2 + uv.y * nv1);
    ctx->meshPosition.sample.direction = glm::normalize(ctx->meshPosition.sample.direction);

    if (ctx->mode == lightmapper::Mode::Thickness) {
        // To generate a thickness map instead of an ambient occlusion map, we invert the sampling direction.
        ctx->meshPosition.sample.direction = -ctx->meshPosition.sample.direction;
    }

    float cameraToSurfaceDistance = (1.0f + ctx->hemisphere.cameraToSurfaceDistanceModifier) * ctx->hemisphere.zNear * sqrtf(2.0f);
    ctx->meshPosition.sample.position = ctx->meshPosition.sample.position + ctx->meshPosition.sample.direction * cameraToSurfaceDistance;

    if (glm::any(glm::isinf(ctx->meshPosition.sample.position)) || glm::any(glm::isinf(ctx->meshPosition.sample.direction)) || glm::length(ctx->meshPosition.sample.direction) < 0.5f) // don't allow 0.0f. should always be ~1.0f
        return false;

    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
    if (std::abs(glm::dot(up, ctx->meshPosition.sample.direction)) > 0.8f)
        up = glm::vec3(0.0f, 0.0f, 1.0f);

    // "randomized" rotation with pattern
    glm::vec3 side = glm::normalize(glm::cross(up, ctx->meshPosition.sample.direction));
    up = glm::normalize(glm::cross(side, ctx->meshPosition.sample.direction));
    int rx = ctx->meshPosition.rasterizer.x % 3;
    int ry = ctx->meshPosition.rasterizer.y % 3;
    const float pi = 3.14159265358979f;
    float phi = 2.0f * pi * baseAngles[ry][rx] + 0.1f * ((float)rand() / (float)RAND_MAX);
    ctx->meshPosition.sample.up = glm::normalize(side * cosf(phi) + up * sinf(phi));
    return true;
}

// returns true if a sampling position was found and
// false if we finished rasterizing the current triangle
bool findFirstConservativeTriangleRasterizerPosition(lightmapper::Context* ctx)
{
    while (!trySamplingConservativeTriangleRasterizerPosition(ctx)) {
        moveToNextPotentialConservativeTriangleRasterizerPosition(ctx);
        if (hasConservativeTriangleRasterizerFinished(ctx))
            return false;
    }
    return true;
}

bool findNextConservativeTriangleRasterizerPosition(lightmapper::Context* ctx)
{
    moveToNextPotentialConservativeTriangleRasterizerPosition(ctx);
    return findFirstConservativeTriangleRasterizerPosition(ctx);
}

void writeResultsToLightmap(lightmapper::Context* ctx)
{
    // do the GPU->CPU transfer of downsampled hemispheres
    float* hemi = (float*)calloc(ctx->hemisphere.storage.width * ctx->hemisphere.storage.height, 4 * sizeof(float));
    ctx->gl->glBindTexture(GL_TEXTURE_2D, ctx->hemisphere.storage.texture);
    ctx->gl->glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, hemi);

    // write results to lightmap texture
    for (int y = 0; y < ctx->hemisphere.storage.writePosition.y + (int)ctx->hemisphere.fbHemiCountY; y++) {
        for (int x = 0; x < ctx->hemisphere.storage.width; x++) {
            int index = y * ctx->hemisphere.storage.width + x;
            glm::ivec2 lmUV = ctx->hemisphere.storage.toLightmapLocation[index];
            if (lmUV.x >= 0) {
                float* c = hemi + index * 4;
                float validity = c[3];
                float* lm = ctx->lightmap.data + (lmUV.y * ctx->lightmap.width + lmUV.x) * ctx->lightmap.channels;
                if (!lm[0] && validity > 0.9) {
                    float scale = 1.0f / validity;
                    switch (ctx->lightmap.channels) {
                    case 1:
                        lm[0] = std::max((c[0] + c[1] + c[2]) * scale / 3.0f, FLT_MIN);
                        break;
                    case 2:
                        lm[0] = std::max((c[0] + c[1] + c[2]) * scale / 3.0f, FLT_MIN);
                        lm[1] = 1.0f; // do we want to support this format?
                        break;
                    case 3:
                        lm[0] = std::max(c[0] * scale, FLT_MIN);
                        lm[1] = std::max(c[1] * scale, FLT_MIN);
                        lm[2] = std::max(c[2] * scale, FLT_MIN);
                        break;
                    case 4:
                        lm[0] = std::max(c[0] * scale, FLT_MIN);
                        lm[1] = std::max(c[1] * scale, FLT_MIN);
                        lm[2] = std::max(c[2] * scale, FLT_MIN);
                        lm[3] = 1.0f;
                        break;
                    default:
                        assert(false);
                        break;
                    }
                }
            }
            ctx->hemisphere.storage.toLightmapLocation[index].x = -1; // reset
        }
    }

    free(hemi);
    ctx->hemisphere.storage.writePosition = glm::ivec2(0, 0);
}

bool integrateHemisphereBatch(lightmapper::Context* ctx)
{
    if (!ctx->hemisphere.fbHemiIndex)
        return false; // nothing to do

    ctx->gl->glDisable(GL_DEPTH_TEST);
    ctx->gl->glBindVertexArray(ctx->hemisphere.vao);

    int fbRead = 0;
    int fbWrite = 1;

    // weighted downsampling pass
    int outHemiSize = ctx->hemisphere.size / 2;
    ctx->gl->glBindFramebuffer(GL_FRAMEBUFFER, ctx->hemisphere.fb[fbWrite]);
    ctx->gl->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ctx->hemisphere.fbTexture[fbWrite], 0);
    ctx->gl->glViewport(0, 0, outHemiSize * ctx->hemisphere.fbHemiCountX, outHemiSize * ctx->hemisphere.fbHemiCountY);
    ctx->gl->glUseProgram(ctx->hemisphere.firstPass.programID);
    ctx->gl->glUniform1i(ctx->hemisphere.firstPass.hemispheresTextureID, 0);
    ctx->gl->glActiveTexture(GL_TEXTURE0);
    ctx->gl->glBindTexture(GL_TEXTURE_2D, ctx->hemisphere.fbTexture[fbRead]);
    ctx->gl->glUniform1i(ctx->hemisphere.firstPass.weightsTextureID, 1);
    ctx->gl->glActiveTexture(GL_TEXTURE1);
    ctx->gl->glBindTexture(GL_TEXTURE_2D, ctx->hemisphere.firstPass.weightsTexture);
    ctx->gl->glActiveTexture(GL_TEXTURE0);
    ctx->gl->glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    // downsampling passes
    ctx->gl->glUseProgram(ctx->hemisphere.downsamplePass.programID);
    ctx->gl->glUniform1i(ctx->hemisphere.downsamplePass.hemispheresTextureID, 0);
    while (outHemiSize > 1) {
        std::swap(fbRead, fbWrite);
        outHemiSize /= 2;
        ctx->gl->glBindFramebuffer(GL_FRAMEBUFFER, ctx->hemisphere.fb[fbWrite]);
        ctx->gl->glViewport(0, 0, outHemiSize * ctx->hemisphere.fbHemiCountX, outHemiSize * ctx->hemisphere.fbHemiCountY);
        ctx->gl->glBindTexture(GL_TEXTURE_2D, ctx->hemisphere.fbTexture[fbRead]);
        ctx->gl->glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }

    // copy results to storage texture
    ctx->gl->glBindTexture(GL_TEXTURE_2D, ctx->hemisphere.storage.texture);
    ctx->gl->glCopyTexSubImage2D(GL_TEXTURE_2D, 0,
        ctx->hemisphere.storage.writePosition.x, ctx->hemisphere.storage.writePosition.y,
        0, 0, ctx->hemisphere.fbHemiCountX, ctx->hemisphere.fbHemiCountY);
    ctx->gl->glBindTexture(GL_TEXTURE_2D, 0);
    ctx->gl->glBindFramebuffer(GL_FRAMEBUFFER, 0);
    ctx->gl->glBindVertexArray(0);
    ctx->gl->glEnable(GL_DEPTH_TEST);

    // copy position mapping to storage
    for (unsigned int y = 0; y < ctx->hemisphere.fbHemiCountY; y++) {
        int sy = ctx->hemisphere.storage.writePosition.y + y;
        for (unsigned int x = 0; x < ctx->hemisphere.fbHemiCountX; x++) {
            int sx = ctx->hemisphere.storage.writePosition.x + x;
            unsigned int hemiIndex = y * ctx->hemisphere.fbHemiCountX + x;
            ctx->hemisphere.storage.toLightmapLocation[sy * ctx->hemisphere.storage.width + sx] = (hemiIndex >= ctx->hemisphere.fbHemiIndex) ? glm::ivec2(-1, -1) : ctx->hemisphere.fbHemiToLightmapLocation[hemiIndex];
        }
    }

    bool needWrite = true;
    // advance storage texture write position
    ctx->hemisphere.storage.writePosition.x += ctx->hemisphere.fbHemiCountX;
    if (ctx->hemisphere.storage.writePosition.x + (int)ctx->hemisphere.fbHemiCountX > ctx->hemisphere.storage.width) {
        ctx->hemisphere.storage.writePosition.x = 0;
        // storage is full
        if (ctx->hemisphere.storage.writePosition.y + (int)ctx->hemisphere.fbHemiCountY >= ctx->hemisphere.storage.height) {
            writeResultsToLightmap(ctx); // read storage data from gpu memory and write it to the lightmap
            needWrite = false;
        } else {
            ctx->hemisphere.storage.writePosition.y += ctx->hemisphere.fbHemiCountY;
        }
    }

    ctx->hemisphere.fbHemiIndex = 0;
    return needWrite;
}

void setView(
    int* viewport, int x, int y, int w, int h,
    float* view, glm::vec3 pos, glm::vec3 dir, glm::vec3 up,
    float* proj, float l, float r, float b, float t, float n, float f)
{
    // viewport
    viewport[0] = x;
    viewport[1] = y;
    viewport[2] = w;
    viewport[3] = h;

    // view matrix: lookAt(pos, pos + dir, up)
    glm::vec3 side = glm::cross(dir, up);
    // up = cross(side, dir);
    dir = -dir;
    pos = -pos;
    view[0] = side.x;
    view[1] = up.x;
    view[2] = dir.x;
    view[3] = 0.0f;
    view[4] = side.y;
    view[5] = up.y;
    view[6] = dir.y;
    view[7] = 0.0f;
    view[8] = side.z;
    view[9] = up.z;
    view[10] = dir.z;
    view[11] = 0.0f;
    view[12] = glm::dot(side, pos);
    view[13] = glm::dot(up, pos);
    view[14] = glm::dot(dir, pos);
    view[15] = 1.0f;

    // projection matrix: frustum(l, r, b, t, n, f)
    float ilr = 1.0f / (r - l), ibt = 1.0f / (t - b), ninf = -1.0f / (f - n), n2 = 2.0f * n;
    proj[0] = n2 * ilr;
    proj[1] = 0.0f;
    proj[2] = 0.0f;
    proj[3] = 0.0f;
    proj[4] = 0.0f;
    proj[5] = n2 * ibt;
    proj[6] = 0.0f;
    proj[7] = 0.0f;
    proj[8] = (r + l) * ilr;
    proj[9] = (t + b) * ibt;
    proj[10] = (f + n) * ninf;
    proj[11] = -1.0f;
    proj[12] = 0.0f;
    proj[13] = 0.0f;
    proj[14] = f * n2 * ninf;
    proj[15] = 0.0f;
}

// returns true if a hemisphere side was prepared for rendering and
// false if we finished the current hemisphere
bool beginSampleHemisphere(lightmapper::Context* ctx, int* viewport, float* view, float* proj)
{
    if (ctx->meshPosition.hemisphere.side >= 5)
        return false;

    if (ctx->meshPosition.hemisphere.side == 0) {
        // prepare hemisphere
        ctx->gl->glBindFramebuffer(GL_FRAMEBUFFER, ctx->hemisphere.fb[0]);
        if (ctx->hemisphere.fbHemiIndex == 0) {
            // prepare hemisphere batch
            ctx->gl->glClearColor( // clear to valid background pixels!
                ctx->hemisphere.clearColor.r,
                ctx->hemisphere.clearColor.g,
                ctx->hemisphere.clearColor.b, 1.0f);
            ctx->gl->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        }
        ctx->hemisphere.fbHemiToLightmapLocation[ctx->hemisphere.fbHemiIndex] = glm::ivec2(ctx->meshPosition.rasterizer.x, ctx->meshPosition.rasterizer.y);
    }

    // find the target position in the batch
    int x = (ctx->hemisphere.fbHemiIndex % ctx->hemisphere.fbHemiCountX) * ctx->hemisphere.size * 3;
    int y = (ctx->hemisphere.fbHemiIndex / ctx->hemisphere.fbHemiCountX) * ctx->hemisphere.size;

    int size = ctx->hemisphere.size;
    float zNear = ctx->hemisphere.zNear;
    float zFar = ctx->hemisphere.zFar;

    glm::vec3 pos = ctx->meshPosition.sample.position;
    glm::vec3 dir = ctx->meshPosition.sample.direction;
    glm::vec3 up = ctx->meshPosition.sample.up;
    glm::vec3 right = glm::cross(dir, up);

    // find the view parameters of the hemisphere side that we will render next
    // hemisphere layout in the framebuffer:
    //       +-------+---+---+-------+
    //       |       |   |   |   D   |
    //       |   C   | R | L +-------+
    //       |       |   |   |   U   |
    //       +-------+---+---+-------+
    switch (ctx->meshPosition.hemisphere.side) {
    case 0: // center
        setView(viewport, x, y, size, size,
            view, pos, dir, up,
            proj, -zNear, zNear, -zNear, zNear, zNear, zFar);
        break;
    case 1: // right
        setView(viewport, size + x, y, size / 2, size,
            view, pos, right, up,
            proj, -zNear, 0.0f, -zNear, zNear, zNear, zFar);
        break;
    case 2: // left
        setView(viewport, size + x + size / 2, y, size / 2, size,
            view, pos, -right, up,
            proj, 0.0f, zNear, -zNear, zNear, zNear, zFar);
        break;
    case 3: // down
        setView(viewport, 2 * size + x, y + size / 2, size, size / 2,
            view, pos, -up, dir,
            proj, -zNear, zNear, 0.0f, zNear, zNear, zFar);
        break;
    case 4: // up
        setView(viewport, 2 * size + x, y, size, size / 2,
            view, pos, up, -dir,
            proj, -zNear, zNear, -zNear, 0.0f, zNear, zFar);
        break;
    default:
        assert(false);
        break;
    }

    return true;
}

void endSampleHemisphere(lightmapper::Context* ctx)
{
    if (++ctx->meshPosition.hemisphere.side == 5) {
        // finish hemisphere
        ctx->gl->glBindFramebuffer(GL_FRAMEBUFFER, 0);
        if (++ctx->hemisphere.fbHemiIndex == ctx->hemisphere.fbHemiCountX * ctx->hemisphere.fbHemiCountY) {
            // downsample new hemisphere batch and store the results
            integrateHemisphereBatch(ctx);
        }
    }
}

void inverseTranspose(const float* m44, float* n33)
{
    if (!m44) {
        n33[0] = 1.0f;
        n33[1] = 0.0f;
        n33[2] = 0.0f;
        n33[3] = 0.0f;
        n33[4] = 1.0f;
        n33[5] = 0.0f;
        n33[6] = 0.0f;
        n33[7] = 0.0f;
        n33[8] = 1.0f;
        return;
    }

    float determinant = m44[0] * (m44[5] * m44[10] - m44[9] * m44[6])
        - m44[1] * (m44[4] * m44[10] - m44[6] * m44[8])
        + m44[2] * (m44[4] * m44[9] - m44[5] * m44[8]);

    assert(fabs(determinant) > FLT_EPSILON);
    float rcpDeterminant = 1.0f / determinant;

    n33[0] = (m44[5] * m44[10] - m44[9] * m44[6]) * rcpDeterminant;
    n33[3] = -(m44[1] * m44[10] - m44[2] * m44[9]) * rcpDeterminant;
    n33[6] = (m44[1] * m44[6] - m44[2] * m44[5]) * rcpDeterminant;
    n33[1] = -(m44[4] * m44[10] - m44[6] * m44[8]) * rcpDeterminant;
    n33[4] = (m44[0] * m44[10] - m44[2] * m44[8]) * rcpDeterminant;
    n33[7] = -(m44[0] * m44[6] - m44[4] * m44[2]) * rcpDeterminant;
    n33[2] = (m44[4] * m44[9] - m44[8] * m44[5]) * rcpDeterminant;
    n33[5] = -(m44[0] * m44[9] - m44[8] * m44[1]) * rcpDeterminant;
    n33[8] = (m44[0] * m44[5] - m44[4] * m44[1]) * rcpDeterminant;
}

glm::vec3 transformNormal(const float* m, glm::vec3 n)
{
    glm::vec3 r;
    r.x = m[0] * n.x + m[3] * n.y + m[6] * n.z;
    r.y = m[1] * n.x + m[4] * n.y + m[7] * n.z;
    r.z = m[2] * n.x + m[5] * n.y + m[8] * n.z;
    return r;
}

glm::vec3 transformPosition(const float* m, glm::vec3 v)
{
    if (!m)
        return v;
    glm::vec3 r;
    r.x = m[0] * v.x + m[4] * v.y + m[8] * v.z + m[12];
    r.y = m[1] * v.x + m[5] * v.y + m[9] * v.z + m[13];
    r.z = m[2] * v.x + m[6] * v.y + m[10] * v.z + m[14];
    float d = m[3] * v.x + m[7] * v.y + m[11] * v.z + m[15];
    assert(std::abs(d - 1.0f) < 0.00001f); // could divide by d, but this shouldn't be a projection transform!
    return r;
}

void setMeshPosition(lightmapper::Context* ctx, unsigned int indicesTriangleBaseIndex)
{
    // fetch triangle at the specified indicesTriangleBaseIndex
    ctx->meshPosition.triangle.baseIndex = indicesTriangleBaseIndex;

    // load and transform triangle to process next
    glm::vec2 uvMin = glm::vec2(FLT_MAX, FLT_MAX), uvMax = glm::vec2(-FLT_MAX, -FLT_MAX);
    glm::vec2 uvScale = glm::ivec2(ctx->lightmap.width, ctx->lightmap.height);
    unsigned int vIndices[3];
    for (int i = 0; i < 3; i++) {
        // decode index
        unsigned int vIndex;
        switch (ctx->mesh.indicesType) {
        case lightmapper::Type::None:
            vIndex = ctx->meshPosition.triangle.baseIndex + i;
            break;
        case lightmapper::Type::UnsignedByte:
            vIndex = ((const unsigned char*)ctx->mesh.indices + ctx->meshPosition.triangle.baseIndex)[i];
            break;
        case lightmapper::Type::UnsignedShort:
            vIndex = ((const unsigned short*)ctx->mesh.indices + ctx->meshPosition.triangle.baseIndex)[i];
            break;
        case lightmapper::Type::UnsignedInt:
            vIndex = ((const unsigned int*)ctx->mesh.indices + ctx->meshPosition.triangle.baseIndex)[i];
            break;
        default:
            assert(false);
            break;
        }
        vIndices[i] = vIndex;

        // decode and pre-transform vertex position
        const void* pPtr = ctx->mesh.positions + vIndex * ctx->mesh.positionsStride;
        glm::vec3 p;
        switch (ctx->mesh.positionsType) {
        // TODO: signed formats
        case lightmapper::Type::UnsignedByte: {
            const unsigned char* uc = (const unsigned char*)pPtr;
            p = glm::vec3(uc[0], uc[1], uc[2]);
        } break;
        case lightmapper::Type::UnsignedShort: {
            const unsigned short* us = (const unsigned short*)pPtr;
            p = glm::vec3(us[0], us[1], us[2]);
        } break;
        case lightmapper::Type::UnsignedInt: {
            const unsigned int* ui = (const unsigned int*)pPtr;
            p = glm::vec3((float)ui[0], (float)ui[1], (float)ui[2]);
        } break;
        case lightmapper::Type::Float: {
            p = *(const glm::vec3*)pPtr;
        } break;
        default: {
            assert(false);
        } break;
        }
        ctx->meshPosition.triangle.p[i] = transformPosition(ctx->mesh.modelMatrix, p);

        // decode and scale (to lightmap resolution) vertex lightmap texture coords
        const void* uvPtr = ctx->mesh.uvs + vIndex * ctx->mesh.uvsStride;
        glm::vec2 uv;
        switch (ctx->mesh.uvsType) {
        case lightmapper::Type::UnsignedByte: {
            const unsigned char* uc = (const unsigned char*)uvPtr;
            uv = glm::vec2(uc[0] / (float)UCHAR_MAX, uc[1] / (float)UCHAR_MAX);
        } break;
        case lightmapper::Type::UnsignedShort: {
            const unsigned short* us = (const unsigned short*)uvPtr;
            uv = glm::vec2(us[0] / (float)USHRT_MAX, us[1] / (float)USHRT_MAX);
        } break;
        case lightmapper::Type::UnsignedInt: {
            const unsigned int* ui = (const unsigned int*)uvPtr;
            uv = glm::vec2(ui[0] / (float)UINT_MAX, ui[1] / (float)UINT_MAX);
        } break;
        case lightmapper::Type::Float: {
            uv = *(const glm::vec2*)uvPtr;
        } break;
        default: {
            assert(false);
        } break;
        }

        ctx->meshPosition.triangle.uv[i] = uvScale * glm::vec2(pmodf(uv.x, 1.0f), pmodf(uv.y, 1.0f)); // maybe clamp to 0.0-1.0 instead of pmod?

        // update bounds on lightmap
        uvMin = glm::min(uvMin, ctx->meshPosition.triangle.uv[i]);
        uvMax = glm::max(uvMax, ctx->meshPosition.triangle.uv[i]);
    }

    glm::vec3 flatNormal = glm::cross(
        ctx->meshPosition.triangle.p[1] - ctx->meshPosition.triangle.p[0],
        ctx->meshPosition.triangle.p[2] - ctx->meshPosition.triangle.p[0]);

    for (int i = 0; i < 3; i++) {
        // decode and pre-transform vertex normal
        const void* nPtr = ctx->mesh.normals + vIndices[i] * ctx->mesh.normalsStride;
        glm::vec3 n;
        switch (ctx->mesh.normalsType) {
        // TODO: signed formats
        case lightmapper::Type::Float: {
            n = *(const glm::vec3*)nPtr;
        } break;
        case lightmapper::Type::None: {
            n = flatNormal;
        } break;
        default: {
            assert(false);
        } break;
        }
        ctx->meshPosition.triangle.n[i] = glm::normalize(transformNormal(ctx->mesh.normalMatrix, n));
    }

    // calculate area of interest (on lightmap) for conservative rasterization
    glm::vec2 bbMin = glm::floor(uvMin);
    glm::vec2 bbMax = glm::ceil(uvMax);
    ctx->meshPosition.rasterizer.minx = std::max((int)bbMin.x - 1, 0);
    ctx->meshPosition.rasterizer.miny = std::max((int)bbMin.y - 1, 0);
    ctx->meshPosition.rasterizer.maxx = std::min((int)bbMax.x + 1, ctx->lightmap.width - 1);
    ctx->meshPosition.rasterizer.maxy = std::min((int)bbMax.y + 1, ctx->lightmap.height - 1);
    assert(ctx->meshPosition.rasterizer.minx <= ctx->meshPosition.rasterizer.maxx && ctx->meshPosition.rasterizer.miny <= ctx->meshPosition.rasterizer.maxy);
    ctx->meshPosition.rasterizer.x = ctx->meshPosition.rasterizer.minx + passOffsetX(ctx);
    ctx->meshPosition.rasterizer.y = ctx->meshPosition.rasterizer.miny + passOffsetY(ctx);

    // try moving to first valid sample position
    if (ctx->meshPosition.rasterizer.x <= ctx->meshPosition.rasterizer.maxx && ctx->meshPosition.rasterizer.y <= ctx->meshPosition.rasterizer.maxy && findFirstConservativeTriangleRasterizerPosition(ctx))
        ctx->meshPosition.hemisphere.side = 0; // we can start sampling the hemisphere
    else
        ctx->meshPosition.hemisphere.side = 5; // no samples on this triangle! put hemisphere sampler into finished state
}

GLuint LoadShader(QOpenGLFunctions_4_1_Core* gl, GLenum type, const char* source)
{
    GLuint shader = gl->glCreateShader(type);
    gl->glShaderSource(shader, 1, &source, NULL);
    gl->glCompileShader(shader);
    GLint compiled;
    gl->glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        return 0;
    }
    return shader;
}

GLuint LoadProgram(QOpenGLFunctions_4_1_Core* gl, const char* vp, const char* fp)
{
    GLuint program = gl->glCreateProgram();
    GLuint vertexShader = LoadShader(gl, GL_VERTEX_SHADER, vp);
    GLuint fragmentShader = LoadShader(gl, GL_FRAGMENT_SHADER, fp);
    gl->glAttachShader(program, vertexShader);
    gl->glAttachShader(program, fragmentShader);
    gl->glLinkProgram(program);
    gl->glDeleteShader(vertexShader);
    gl->glDeleteShader(fragmentShader);
    GLint linked;
    gl->glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
        return 0;
    }
    return program;
}

}

lightmapper::Context* lightmapper::create(
    QOpenGLFunctions_4_1_Core* gl,
    Mode mode,
    int hemisphereSize, float zNear, float zFar,
    float clearR, float clearG, float clearB,
    int interpolationPasses, float interpolationThreshold,
    float cameraToSurfaceDistanceModifier)
{
    assert(hemisphereSize == 512 || hemisphereSize == 256 || hemisphereSize == 128 || hemisphereSize == 64 || hemisphereSize == 32 || hemisphereSize == 16);
    assert(zNear < zFar && zNear > 0.0f);
    assert(cameraToSurfaceDistanceModifier >= -1.0f);
    assert(interpolationPasses >= 0 && interpolationPasses <= 8);
    assert(interpolationThreshold >= 0.0f);

    Context* ctx = (Context*)calloc(1, sizeof(Context));
    ctx->gl = gl;

    ctx->meshPosition.passCount = 1 + 3 * interpolationPasses;
    ctx->interpolationThreshold = interpolationThreshold;
    ctx->hemisphere.size = hemisphereSize;
    ctx->hemisphere.zNear = zNear;
    ctx->hemisphere.zFar = zFar;
    ctx->hemisphere.cameraToSurfaceDistanceModifier = cameraToSurfaceDistanceModifier;
    ctx->hemisphere.clearColor.r = clearR;
    ctx->hemisphere.clearColor.g = clearG;
    ctx->hemisphere.clearColor.b = clearB;

    // calculate hemisphere batch size
    ctx->hemisphere.fbHemiCountX = 1536 / (3 * ctx->hemisphere.size);
    ctx->hemisphere.fbHemiCountY = 512 / ctx->hemisphere.size;

    // hemisphere batch framebuffers
    unsigned int w[] = {
        ctx->hemisphere.fbHemiCountX * ctx->hemisphere.size * 3,
        ctx->hemisphere.fbHemiCountX * ctx->hemisphere.size / 2
    };
    unsigned int h[] = {
        ctx->hemisphere.fbHemiCountY * ctx->hemisphere.size,
        ctx->hemisphere.fbHemiCountY * ctx->hemisphere.size / 2
    };

    ctx->gl->glGenTextures(2, ctx->hemisphere.fbTexture);
    ctx->gl->glGenFramebuffers(2, ctx->hemisphere.fb);
    ctx->gl->glGenRenderbuffers(1, &ctx->hemisphere.fbDepth);

    ctx->gl->glBindRenderbuffer(GL_RENDERBUFFER, ctx->hemisphere.fbDepth);
    ctx->gl->glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w[0], h[0]);
    ctx->gl->glBindFramebuffer(GL_FRAMEBUFFER, ctx->hemisphere.fb[0]);
    ctx->gl->glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, ctx->hemisphere.fbDepth);
    for (int i = 0; i < 2; i++) {
        ctx->gl->glBindTexture(GL_TEXTURE_2D, ctx->hemisphere.fbTexture[i]);
        ctx->gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        ctx->gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        ctx->gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        ctx->gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        ctx->gl->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, w[i], h[i], 0, GL_RGBA, GL_FLOAT, 0);

        ctx->gl->glBindFramebuffer(GL_FRAMEBUFFER, ctx->hemisphere.fb[i]);
        ctx->gl->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ctx->hemisphere.fbTexture[i], 0);
        GLenum status = ctx->gl->glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
            ctx->gl->glDeleteRenderbuffers(1, &ctx->hemisphere.fbDepth);
            ctx->gl->glDeleteFramebuffers(2, ctx->hemisphere.fb);
            ctx->gl->glDeleteTextures(2, ctx->hemisphere.fbTexture);
            free(ctx);
            return NULL;
        }
    }
    ctx->gl->glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // dummy vao for fullscreen quad rendering
    ctx->gl->glGenVertexArrays(1, &ctx->hemisphere.vao);

    // hemisphere shader (weighted downsampling of the 3x1 hemisphere layout to a 0.5x0.5 square)
    {
        const char* vs = "#version 150 core\n"
                         "const vec2 ps[4] = vec2[](vec2(1, -1), vec2(1, 1), vec2(-1, -1), vec2(-1, 1));\n"
                         "void main()\n"
                         "{\n"
                         "gl_Position = vec4(ps[gl_VertexID], 0, 1);\n"
                         "}\n";
        const char* fs = "#version 150 core\n"
                         "uniform sampler2D hemispheres;\n"
                         "uniform sampler2D weights;\n"

                         "layout(pixel_center_integer) in vec4 gl_FragCoord;\n" // whole integer values represent pixel centers, GL_ARB_fragment_coord_conventions

                         "out vec4 outColor;\n"

                         "vec4 weightedSample(ivec2 h_uv, ivec2 w_uv, ivec2 quadrant)\n"
                         "{\n"
                         "vec4 sample = texelFetch(hemispheres, h_uv + quadrant, 0);\n"
                         "vec2 weight = texelFetch(weights, w_uv + quadrant, 0).rg;\n"
                         "return vec4(sample.rgb * weight.r, sample.a * weight.g);\n"
                         "}\n"

                         "vec4 threeWeightedSamples(ivec2 h_uv, ivec2 w_uv, ivec2 offset)\n"
                         "{\n" // horizontal triple sum
                         "vec4 sum = weightedSample(h_uv, w_uv, offset);\n"
                         "sum += weightedSample(h_uv, w_uv, offset + ivec2(2, 0));\n"
                         "sum += weightedSample(h_uv, w_uv, offset + ivec2(4, 0));\n"
                         "return sum;\n"
                         "}\n"

                         "void main()\n"
                         "{\n" // this is a weighted sum downsampling pass (alpha component contains the weighted valid sample count)
                         "vec2 in_uv = gl_FragCoord.xy * vec2(6.0, 2.0) + vec2(0.5);\n"
                         "ivec2 h_uv = ivec2(in_uv);\n"
                         "ivec2 w_uv = ivec2(mod(in_uv, vec2(textureSize(weights, 0))));\n" // there's no integer modulo :(
                         "vec4 lb = threeWeightedSamples(h_uv, w_uv, ivec2(0, 0));\n"
                         "vec4 rb = threeWeightedSamples(h_uv, w_uv, ivec2(1, 0));\n"
                         "vec4 lt = threeWeightedSamples(h_uv, w_uv, ivec2(0, 1));\n"
                         "vec4 rt = threeWeightedSamples(h_uv, w_uv, ivec2(1, 1));\n"
                         "outColor = lb + rb + lt + rt;\n"
                         "}\n";
        ctx->hemisphere.firstPass.programID = LoadProgram(gl, vs, fs);
        if (!ctx->hemisphere.firstPass.programID) {
            ctx->gl->glDeleteVertexArrays(1, &ctx->hemisphere.vao);
            ctx->gl->glDeleteRenderbuffers(1, &ctx->hemisphere.fbDepth);
            ctx->gl->glDeleteFramebuffers(2, ctx->hemisphere.fb);
            ctx->gl->glDeleteTextures(2, ctx->hemisphere.fbTexture);
            free(ctx);
            return NULL;
        }
        ctx->hemisphere.firstPass.hemispheresTextureID = ctx->gl->glGetUniformLocation(ctx->hemisphere.firstPass.programID, "hemispheres");
        ctx->hemisphere.firstPass.weightsTextureID = ctx->gl->glGetUniformLocation(ctx->hemisphere.firstPass.programID, "weights");
    }

    // downsample shader
    {
        const char* vs = "#version 150 core\n"
                         "const vec2 ps[4] = vec2[](vec2(1, -1), vec2(1, 1), vec2(-1, -1), vec2(-1, 1));\n"
                         "void main()\n"
                         "{\n"
                         "gl_Position = vec4(ps[gl_VertexID], 0, 1);\n"
                         "}\n";
        const char* fs = "#version 150 core\n"
                         "uniform sampler2D hemispheres;\n"

                         "layout(pixel_center_integer) in vec4 gl_FragCoord;\n" // whole integer values represent pixel centers, GL_ARB_fragment_coord_conventions

                         "out vec4 outColor;\n"

                         "void main()\n"
                         "{\n" // this is a sum downsampling pass (alpha component contains the weighted valid sample count)
                         "ivec2 h_uv = ivec2(gl_FragCoord.xy) * 2;\n"
                         "vec4 lb = texelFetch(hemispheres, h_uv + ivec2(0, 0), 0);\n"
                         "vec4 rb = texelFetch(hemispheres, h_uv + ivec2(1, 0), 0);\n"
                         "vec4 lt = texelFetch(hemispheres, h_uv + ivec2(0, 1), 0);\n"
                         "vec4 rt = texelFetch(hemispheres, h_uv + ivec2(1, 1), 0);\n"
                         "outColor = lb + rb + lt + rt;\n"
                         "}\n";
        ctx->hemisphere.downsamplePass.programID = LoadProgram(gl, vs, fs);
        if (!ctx->hemisphere.downsamplePass.programID) {
            ctx->gl->glDeleteProgram(ctx->hemisphere.firstPass.programID);
            ctx->gl->glDeleteVertexArrays(1, &ctx->hemisphere.vao);
            ctx->gl->glDeleteRenderbuffers(1, &ctx->hemisphere.fbDepth);
            ctx->gl->glDeleteFramebuffers(2, ctx->hemisphere.fb);
            ctx->gl->glDeleteTextures(2, ctx->hemisphere.fbTexture);
            free(ctx);
            return NULL;
        }
        ctx->hemisphere.downsamplePass.hemispheresTextureID = ctx->gl->glGetUniformLocation(ctx->hemisphere.downsamplePass.programID, "hemispheres");
    }

    // hemisphere weights texture
    ctx->gl->glGenTextures(1, &ctx->hemisphere.firstPass.weightsTexture);
    setHemisphereWeights(ctx);
    ctx->gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    ctx->gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    ctx->gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    ctx->gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    // allocate batchPosition-to-lightmapPosition map
    ctx->hemisphere.fbHemiToLightmapLocation = (glm::ivec2*)calloc(ctx->hemisphere.fbHemiCountX * ctx->hemisphere.fbHemiCountY, sizeof(glm::ivec2));

    ctx->mode = mode;

    return ctx;
}

void lightmapper::destroy(lightmapper::Context* ctx)
{
    // reset state
    ctx->gl->glUseProgram(0);
    ctx->gl->glBindTexture(GL_TEXTURE_2D, 0);
    ctx->gl->glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    ctx->gl->glBindVertexArray(0);
    ctx->gl->glBindFramebuffer(GL_FRAMEBUFFER, 0);
    ctx->gl->glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    ctx->gl->glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

    // delete gl objects
    ctx->gl->glDeleteTextures(1, &ctx->hemisphere.firstPass.weightsTexture);
    ctx->gl->glDeleteTextures(1, &ctx->hemisphere.storage.texture);
    ctx->gl->glDeleteProgram(ctx->hemisphere.downsamplePass.programID);
    ctx->gl->glDeleteProgram(ctx->hemisphere.firstPass.programID);
    ctx->gl->glDeleteVertexArrays(1, &ctx->hemisphere.vao);
    ctx->gl->glDeleteRenderbuffers(1, &ctx->hemisphere.fbDepth);
    ctx->gl->glDeleteFramebuffers(2, ctx->hemisphere.fb);
    ctx->gl->glDeleteTextures(2, ctx->hemisphere.fbTexture);
    ctx->gl->glDeleteTextures(1, &ctx->hemisphere.storage.texture);

    // free memory
    free(ctx->hemisphere.storage.toLightmapLocation);
    free(ctx->hemisphere.fbHemiToLightmapLocation);
    free(ctx);
}

void lightmapper::setHemisphereWeights(lightmapper::Context* ctx)
{
    // hemisphere weights texture. bakes in material dependent attenuation behaviour.
    float* weights = (float*)calloc(2 * 3 * ctx->hemisphere.size * ctx->hemisphere.size, sizeof(float));
    float center = (ctx->hemisphere.size - 1) * 0.5f;
    double sum = 0.0;
    for (unsigned int y = 0; y < ctx->hemisphere.size; y++) {
        float dy = 2.0f * (y - center) / (float)ctx->hemisphere.size;
        for (unsigned int x = 0; x < ctx->hemisphere.size; x++) {
            float dx = 2.0f * (x - center) / (float)ctx->hemisphere.size;
            glm::vec3 v = glm::normalize(glm::vec3(dx, dy, 1.0f));

            float solidAngle = v.z * v.z * v.z;

            float* w0 = weights + 2 * (y * (3 * ctx->hemisphere.size) + x);
            float* w1 = w0 + 2 * ctx->hemisphere.size;
            float* w2 = w1 + 2 * ctx->hemisphere.size;

            // center weights
            w0[0] = solidAngle;
            w0[1] = solidAngle;

            // left/right side weights
            w1[0] = solidAngle;
            w1[1] = solidAngle;

            // up/down side weights
            w2[0] = solidAngle;
            w2[1] = solidAngle;

            sum += 3.0 * (double)solidAngle;
        }
    }

    // normalize weights
    float weightScale = (float)(1.0 / sum);
    for (unsigned int i = 0; i < 2 * 3 * ctx->hemisphere.size * ctx->hemisphere.size; i++)
        weights[i] *= weightScale;

    // upload weight texture
    ctx->gl->glBindTexture(GL_TEXTURE_2D, ctx->hemisphere.firstPass.weightsTexture);
    ctx->gl->glTexImage2D(GL_TEXTURE_2D, 0, GL_RG32F, 3 * ctx->hemisphere.size, ctx->hemisphere.size, 0, GL_RG, GL_FLOAT, weights);
    free(weights);
}

void lightmapper::setTargetLightmap(lightmapper::Context* ctx, float* outLightmap, int w, int h, int c)
{
    ctx->lightmap.data = outLightmap;
    ctx->lightmap.width = w;
    ctx->lightmap.height = h;
    ctx->lightmap.channels = c;

    unsigned int sw = w > ctx->hemisphere.fbHemiCountX ? w : ctx->hemisphere.fbHemiCountX;
    unsigned int sh = h > ctx->hemisphere.fbHemiCountY ? h : ctx->hemisphere.fbHemiCountY;

    // allocate storage texture
    if (!ctx->hemisphere.storage.texture)
        ctx->gl->glGenTextures(1, &ctx->hemisphere.storage.texture);
    ctx->hemisphere.storage.width = sw;
    ctx->hemisphere.storage.height = sh;
    ctx->gl->glBindTexture(GL_TEXTURE_2D, ctx->hemisphere.storage.texture);
    ctx->gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    ctx->gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    ctx->gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    ctx->gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    ctx->gl->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, sw, sh, 0, GL_RGBA, GL_FLOAT, 0);

    // allocate storage position to lightmap position map
    if (ctx->hemisphere.storage.toLightmapLocation)
        free(ctx->hemisphere.storage.toLightmapLocation);
    ctx->hemisphere.storage.toLightmapLocation = (glm::ivec2*)calloc(sw * sh, sizeof(glm::ivec2));
    // invalidate all positions
    for (int i = 0; i < sw * sh; i++)
        ctx->hemisphere.storage.toLightmapLocation[i].x = -1;
}

void lightmapper::setGeometry(lightmapper::Context* ctx,
    const float* transformationMatrix,
    Type positionsType, const void* positionsXYZ, int positionsStride,
    Type normalsType, const void* normalsXYZ, int normalsStride,
    Type texCoordsType, const void* texCoordsUV, int texCoordsStride,
    int count, Type indicesType, const void* indices)
{
    ctx->mesh.modelMatrix = transformationMatrix;
    ctx->mesh.positions = (const unsigned char*)positionsXYZ;
    ctx->mesh.positionsType = positionsType;
    ctx->mesh.positionsStride = positionsStride == 0 ? sizeof(glm::vec3) : positionsStride;
    ctx->mesh.normals = (const unsigned char*)normalsXYZ;
    ctx->mesh.normalsType = normalsType;
    ctx->mesh.normalsStride = normalsStride == 0 ? sizeof(glm::vec3) : normalsStride;
    ctx->mesh.uvs = (const unsigned char*)texCoordsUV;
    ctx->mesh.uvsType = texCoordsType;
    ctx->mesh.uvsStride = texCoordsStride == 0 ? sizeof(glm::vec2) : texCoordsStride;
    ctx->mesh.indicesType = indicesType;
    ctx->mesh.indices = (const unsigned char*)indices;
    ctx->mesh.count = count;

    inverseTranspose(transformationMatrix, ctx->mesh.normalMatrix);

    ctx->meshPosition.pass = 0;
    setMeshPosition(ctx, 0);
}

bool lightmapper::begin(lightmapper::Context* ctx, int* outViewport4, float* outView4x4, float* outProjection4x4)
{
    assert(ctx->meshPosition.triangle.baseIndex < ctx->mesh.count);
    while (!beginSampleHemisphere(ctx, outViewport4, outView4x4, outProjection4x4)) { // as long as there are no hemisphere sides to render...
        // try moving to the next rasterizer position
        if (findNextConservativeTriangleRasterizerPosition(ctx)) { // if we successfully moved to the next sample position on the current triangle...
            ctx->meshPosition.hemisphere.side = 0; // start sampling a hemisphere there
        } else { // if there are no valid sample positions on the current triangle...
            if (ctx->meshPosition.triangle.baseIndex + 3 < ctx->mesh.count) { // ...and there are triangles left: move to the next triangle and continue sampling.
                setMeshPosition(ctx, ctx->meshPosition.triangle.baseIndex + 3);
            } else { // ...and there are no triangles left: finish
                if (integrateHemisphereBatch(ctx)) // integrate and store last batch
                    writeResultsToLightmap(ctx); // read storage data from gpu memory and write it to the lightmap

                if (++ctx->meshPosition.pass == ctx->meshPosition.passCount) {
                    ctx->meshPosition.pass = 0;
                    ctx->meshPosition.triangle.baseIndex = ctx->mesh.count; // set end condition (in case someone accidentally calls lmBegin again)
                    return false;
                }

                setMeshPosition(ctx, 0); // start over with the next pass
            }
        }
    }
    return true;
}

float lightmapper::progress(lightmapper::Context* ctx)
{
    float passProgress = (float)ctx->meshPosition.triangle.baseIndex / (float)ctx->mesh.count;
    return ((float)ctx->meshPosition.pass + passProgress) / (float)ctx->meshPosition.passCount;
}

void lightmapper::end(lightmapper::Context* ctx)
{
    endSampleHemisphere(ctx);
}

float lightmapper::imageMax(const float* image, int w, int h, int c)
{
    float maxValue = 0.0f;
    for (int i = 0; i < w * h; i++)
        for (int j = 0; j < c; j++)
            maxValue = std::max(maxValue, image[i * c + j]);
    return maxValue;
}

void lightmapper::imageDilate(const float* image, float* outImage, int w, int h, int c)
{
    assert(c > 0 && c <= 4);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            float color[4];
            bool valid = false;
            for (int i = 0; i < c; i++) {
                color[i] = image[(y * w + x) * c + i];
                valid |= color[i] > 0.0f;
            }
            if (!valid) {
                int n = 0;
                const int dx[] = { -1, 0, 1, 0 };
                const int dy[] = { 0, 1, 0, -1 };
                for (int d = 0; d < 4; d++) {
                    int cx = x + dx[d];
                    int cy = y + dy[d];
                    if (cx >= 0 && cx < w && cy >= 0 && cy < h) {
                        float dcolor[4];
                        bool dvalid = false;
                        for (int i = 0; i < c; i++) {
                            dcolor[i] = image[(cy * w + cx) * c + i];
                            dvalid |= dcolor[i] > 0.0f;
                        }
                        if (dvalid) {
                            for (int i = 0; i < c; i++)
                                color[i] += dcolor[i];
                            n++;
                        }
                    }
                }
                if (n) {
                    float in = 1.0f / n;
                    for (int i = 0; i < c; i++)
                        color[i] *= in;
                }
            }
            for (int i = 0; i < c; i++)
                outImage[(y * w + x) * c + i] = color[i];
        }
    }
}

void lightmapper::imageFtoUB(const float* image, unsigned char* outImage, int w, int h, int c, float max)
{
    assert(c > 0);
    float scale = 255.0f / (max != 0.0f ? max : imageMax(image, w, h, c));
    for (int i = 0; i < w * h * c; i++) {
        outImage[i] = (unsigned char)std::clamp(image[i] * scale, 0.0f, 255.0f);
    }
}
