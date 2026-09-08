/////////////////////////////////////////////
//
// Mesh Simplification Tutorial
//
// (C) by Sven Forstmann in 2014
//
// License : MIT
// http://opensource.org/licenses/MIT
//
// https://github.com/sp4cerat/Fast-Quadric-Mesh-Simplification
//
// 5/2016: Chris Rorden created minimal version for OSX/Linux/Windows compile

//#include <iostream>
//#include <stddef.h>
//#include <functional>
//#include <sys/stat.h>
//#include <stdbool.h>
#include <string.h>
//#include <ctype.h>
//#include <float.h>
#include <algorithm>
#include <float.h> //FLT_EPSILON, DBL_EPSILON
#include <limits>
#include <map>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vec2.hpp>
#include <vec3.hpp>
#include <vector>

#include "core/Logger.h"
#include "core/Platform.h"

UX3D_BEGIN_IGNORE_WARNINGS

// TODO: remove defines
#define loopi(start_l, end_l) for (int i = start_l; i < end_l; ++i)
#define loopj(start_l, end_l) for (int j = start_l; j < end_l; ++j)

namespace ux3d::mesh_simplification {

bool interpolateAttributes = true;

glm::vec3 barycentric(const glm::vec3& p, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c)
{
    glm::vec3 v0 = b - a;
    glm::vec3 v1 = c - a;
    glm::vec3 v2 = p - a;
    double d00 = glm::dot(v0, v0);
    double d01 = glm::dot(v0, v1);
    double d11 = glm::dot(v1, v1);
    double d20 = glm::dot(v2, v0);
    double d21 = glm::dot(v2, v1);
    double denom = d00 * d11 - d01 * d01;
    double v = (d11 * d20 - d01 * d21) / denom;
    double w = (d00 * d21 - d01 * d20) / denom;
    double u = 1.0 - v - w;
    return glm::vec3(u, v, w);
}

glm::vec4 interpolate(const glm::vec3& p, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec4 attrs[3])
{
    glm::vec3 bary = barycentric(p, a, b, c);
    glm::vec4 out = glm::vec4(0, 0, 0, 0);
    out = out + attrs[0] * bary.x;
    out = out + attrs[1] * bary.y;
    out = out + attrs[2] * bary.z;
    return out;
}

glm::vec3 interpolate(const glm::vec3& p, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3 attrs[3])
{
    glm::vec3 bary = barycentric(p, a, b, c);
    glm::vec3 out = glm::vec3(0, 0, 0);
    out = out + attrs[0] * bary.x;
    out = out + attrs[1] * bary.y;
    out = out + attrs[2] * bary.z;
    return out;
}

// calculate interpolation weights of point p inside triangle [a,b,c] and apply on corner attributes
glm::vec2 interpolate(const glm::vec3& p, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec2 attrs[3])
{
    glm::vec3 weights = barycentric(p, a, b, c);
    glm::vec2 out = glm::vec2(0, 0);
    out = out + attrs[0] * weights.x;
    out = out + attrs[1] * weights.y;
    out = out + attrs[2] * weights.z;
    return out;
}

class SymetricMatrix {

public:
    // Constructor

    SymetricMatrix(double c = 0) { loopi(0, 10) m[i] = c; }

    SymetricMatrix(double m11, double m12, double m13, double m14,
        double m22, double m23, double m24,
        double m33, double m34,
        double m44)
    {
        m[0] = m11;
        m[1] = m12;
        m[2] = m13;
        m[3] = m14;
        m[4] = m22;
        m[5] = m23;
        m[6] = m24;
        m[7] = m33;
        m[8] = m34;
        m[9] = m44;
    }

    // Make plane

    SymetricMatrix(double a, double b, double c, double d)
    {
        m[0] = a * a;
        m[1] = a * b;
        m[2] = a * c;
        m[3] = a * d;
        m[4] = b * b;
        m[5] = b * c;
        m[6] = b * d;
        m[7] = c * c;
        m[8] = c * d;
        m[9] = d * d;
    }

    double operator[](int c) const { return m[c]; }

    // Determinant

    double det(int a11, int a12, int a13,
        int a21, int a22, int a23,
        int a31, int a32, int a33)
    {
        double det = m[a11] * m[a22] * m[a33] + m[a13] * m[a21] * m[a32] + m[a12] * m[a23] * m[a31]
            - m[a13] * m[a22] * m[a31] - m[a11] * m[a23] * m[a32] - m[a12] * m[a21] * m[a33];
        return det;
    }

    const SymetricMatrix operator+(const SymetricMatrix& n) const
    {
        return SymetricMatrix(m[0] + n[0], m[1] + n[1], m[2] + n[2], m[3] + n[3],
            m[4] + n[4], m[5] + n[5], m[6] + n[6],
            m[7] + n[7], m[8] + n[8],
            m[9] + n[9]);
    }

    SymetricMatrix& operator+=(const SymetricMatrix& n)
    {
        m[0] += n[0];
        m[1] += n[1];
        m[2] += n[2];
        m[3] += n[3];
        m[4] += n[4];
        m[5] += n[5];
        m[6] += n[6];
        m[7] += n[7];
        m[8] += n[8];
        m[9] += n[9];
        return *this;
    }

    double m[10];
};
///////////////////////////////////////////

// Global Variables & Strctures
enum Attributes {
    NONE,
    NORMAL = 2,
    TEXCOORD = 4,
    COLOR = 8
};
struct Triangle {
    int v[3]; // vertex index
    double err[4];
    int deleted, dirty, attr;
    glm::vec3 n;
    glm::vec2 texCoord0[3];
    glm::vec2 texCoord1[3];
    glm::vec3 normal[3];
    glm::vec4 tangent[3];
    glm::vec4 color0[3];
    int material;
};

struct Vertex {
    glm::vec3 position;
    int tstart, tcount;
    SymetricMatrix q;
    int border;
};

struct Ref {
    int tid, tvertex;
};
std::vector<Triangle> triangles;
std::vector<Vertex> vertices;

std::vector<Ref> refs;
std::string mtllib;
std::vector<std::string> materials;

// Helper functions

double vertex_error(SymetricMatrix q, double x, double y, double z);
double calculate_error(int id_v1, int id_v2, glm::vec3& p_result);
bool flipped(glm::vec3 p, int i0, int i1, Vertex& v0, Vertex& v1, std::vector<int>& deleted);
void update_attributes(int i0, Vertex& v, const glm::vec3& p, std::vector<int>& deleted);
void update_triangles(int i0, Vertex& v, std::vector<int>& deleted, int& deleted_triangles);
void update_mesh(int iteration);
void compact_mesh();
//
// Main simplification function
//
// target_count  : target nr. of triangles
// aggressiveness : sharpness to increase the threshold.
//                 5..8 are good numbers
//                 more iterations yield higher quality
//

void simplify_mesh(int target_count, double aggressiveness = 7, bool verbose = false)
{

    int updateCount = 5; // original value = 5

    // init
    loopi(0, triangles.size())
    {
        triangles[i].deleted = 0;
    }

    // main iteration loop
    int deleted_triangles = 0;
    std::vector<int> deleted0, deleted1;
    int triangle_count = triangles.size();

    for (int iteration = 0; iteration < 100; iteration++) {
        if (triangle_count - deleted_triangles <= target_count)
            break;

        // update mesh once in a while
        if (iteration % updateCount == 0) {
            update_mesh(iteration);
        }

        // clear dirty flag
        loopi(0, triangles.size()) triangles[i].dirty = 0;

        //
        // All triangles with edges below the threshold will be removed
        //
        // The following numbers works well for most models.
        // If it does not, try to adjust the 3 parameters
        //
        double threshold = 0.000000001 * pow(double(iteration + 3), aggressiveness);

        double meanError = 0.0;
        double minError = std::numeric_limits<double>::max();
        double maxError = 0.0;

        // target number of triangles reached ? Then break
        if ((verbose) && (iteration % 5 == 0)) {
            ux3d::core::debugLog(ux3d::core::LogLevel::Info, "Simplify: iteration=%d   triangles=%d   threshold=%g", iteration, triangle_count - deleted_triangles, threshold);
        }

        // remove vertices & mark deleted triangles
        loopi(0, triangles.size())
        {
            Triangle& t = triangles[i];

            if (t.deleted)
                continue;
            if (t.dirty)
                continue;

            meanError += t.err[3] / triangles.size();
            minError = std::min(minError, t.err[3]);
            maxError = std::max(maxError, t.err[3]);

            if (t.err[3] > threshold)
                continue;

            loopj(0, 3) if (t.err[j] < threshold)
            {

                int i0 = t.v[j];
                Vertex& v0 = vertices[i0];
                int i1 = t.v[(j + 1) % 3];
                Vertex& v1 = vertices[i1];
                // Border check
                // if(v0.border != v1.border)  continue;
                if (v0.border || v1.border) {
                    continue; // keep border vertices
                }

                // Compute vertex to collapse to
                glm::vec3 p;
                calculate_error(i0, i1, p);
                deleted0.resize(v0.tcount); // normals temporarily
                deleted1.resize(v1.tcount); // normals temporarily
                // don't remove if flipped
                if (flipped(p, i0, i1, v0, v1, deleted0)) {
                    continue;
                }

                if (flipped(p, i1, i0, v1, v0, deleted1)) {
                    continue;
                }

                if (interpolateAttributes) {
                    update_attributes(i0, v0, p, deleted0);
                    update_attributes(i0, v1, p, deleted1);
                }

                // not flipped, so remove edge
                v0.position = p;
                v0.q = v1.q + v0.q;
                int tstart = refs.size();

                update_triangles(i0, v0, deleted0, deleted_triangles);
                update_triangles(i0, v1, deleted1, deleted_triangles);

                int tcount = refs.size() - tstart;

                if (tcount <= v0.tcount) {
                    // save ram
                    if (tcount)
                        memcpy(&refs[v0.tstart], &refs[tstart], tcount * sizeof(Ref));
                } else {
                    // append
                    v0.tstart = tstart;
                }

                v0.tcount = tcount;
                break;
            }

            // done?
            if (triangle_count - deleted_triangles <= target_count) {
                ux3d::core::debugLog(ux3d::core::LogLevel::Info, "Simplify: done");
                break;
            }
        }

        if ((verbose) && (iteration % 5 == 0)) {
            ux3d::core::debugLog(ux3d::core::LogLevel::Info, "Simplify: meanError=%g  minError=%g  maxError=%g", meanError, minError, maxError);
        }
    }
    // clean up mesh
    compact_mesh();
} // simplify_mesh()

void simplify_mesh_lossless(bool verbose = false)
{
    // init
    loopi(0, triangles.size()) triangles[i].deleted = 0;

    // main iteration loop
    int deleted_triangles = 0;
    std::vector<int> deleted0, deleted1;
    int triangle_count = triangles.size();
    // int iteration = 0;
    // loop(iteration,0,100)
    for (int iteration = 0; iteration < 9999; iteration++) {
        // update mesh constantly
        update_mesh(iteration);
        // clear dirty flag
        loopi(0, triangles.size()) triangles[i].dirty = 0;
        //
        // All triangles with edges below the threshold will be removed
        //
        // The following numbers works well for most models.
        // If it does not, try to adjust the 3 parameters
        //
        double threshold = DBL_EPSILON; // 1.0E-3 EPS;
        if (verbose) {
            printf("lossless iteration %d\n", iteration);
        }

        // remove vertices & mark deleted triangles
        loopi(0, triangles.size())
        {
            Triangle& t = triangles[i];
            if (t.err[3] > threshold)
                continue;
            if (t.deleted)
                continue;
            if (t.dirty)
                continue;

            loopj(0, 3) if (t.err[j] < threshold)
            {
                int i0 = t.v[j];
                Vertex& v0 = vertices[i0];
                int i1 = t.v[(j + 1) % 3];
                Vertex& v1 = vertices[i1];

                // Border check
                if (v0.border != v1.border)
                    continue;

                // Compute vertex to collapse to
                glm::vec3 p;
                calculate_error(i0, i1, p);

                deleted0.resize(v0.tcount); // normals temporarily
                deleted1.resize(v1.tcount); // normals temporarily

                // don't remove if flipped
                if (flipped(p, i0, i1, v0, v1, deleted0))
                    continue;
                if (flipped(p, i1, i0, v1, v0, deleted1))
                    continue;

                if (interpolateAttributes) {
                    update_attributes(i0, v0, p, deleted0);
                    update_attributes(i0, v1, p, deleted1);
                }

                // not flipped, so remove edge
                v0.position = p;
                v0.q = v1.q + v0.q;
                int tstart = refs.size();

                update_triangles(i0, v0, deleted0, deleted_triangles);
                update_triangles(i0, v1, deleted1, deleted_triangles);

                int tcount = refs.size() - tstart;

                if (tcount <= v0.tcount) {
                    // save ram
                    if (tcount)
                        memcpy(&refs[v0.tstart], &refs[tstart], tcount * sizeof(Ref));
                } else
                    // append
                    v0.tstart = tstart;

                v0.tcount = tcount;
                break;
            }
        }
        if (deleted_triangles <= 0)
            break;
        deleted_triangles = 0;
    } // for each iteration
    // clean up mesh
    compact_mesh();
} // simplify_mesh_lossless()

// Check if a triangle flips when this edge is removed

bool flipped(glm::vec3 p, int i0, int i1, Vertex& v0, Vertex& v1, std::vector<int>& deleted)
{

    for (int k = 0; k < v0.tcount; ++k) {
        Triangle& t = triangles[refs[v0.tstart + k].tid];
        if (t.deleted)
            continue;

        int s = refs[v0.tstart + k].tvertex;
        int id1 = t.v[(s + 1) % 3];
        int id2 = t.v[(s + 2) % 3];

        if (id1 == i1 || id2 == i1) // delete ?
        {
            deleted[k] = 1;
            continue;
        }

        glm::vec3 d1 = vertices[id1].position - p;
        d1 = glm::normalize(d1);
        glm::vec3 d2 = vertices[id2].position - p;
        d2 = glm::normalize(d2);
        if (abs(glm::dot(d1, d2)) > 0.999) {
            return true;
        }
        glm::vec3 n;
        n = glm::cross(d1, d2);
        n = glm::normalize(n);
        deleted[k] = 0;
        if (glm::dot(n, t.n) < 0.2) {
            return true;
        }
    }
    return false;
}

void update_attributes(int i0, Vertex& vertex, const glm::vec3& p, std::vector<int>& deleted)
{

    for (int k = 0; k < vertex.tcount; ++k) {
        Ref& reference = refs[vertex.tstart + k];
        Triangle& triangle = triangles[reference.tid];
        if (triangle.deleted)
            continue;
        if (deleted[k])
            continue;

        glm::vec3 p1 = vertices[triangle.v[0]].position;
        glm::vec3 p2 = vertices[triangle.v[1]].position;
        glm::vec3 p3 = vertices[triangle.v[2]].position;

        triangle.texCoord0[reference.tvertex] = interpolate(p, p1, p2, p3, triangle.texCoord0);
        triangle.texCoord1[reference.tvertex] = interpolate(p, p1, p2, p3, triangle.texCoord1);
        triangle.normal[reference.tvertex] = glm::normalize(interpolate(p, p1, p2, p3, triangle.normal));

        std::vector<glm::vec3> tangents;
        tangents.push_back(glm::vec3(triangle.tangent[0]));
        tangents.push_back(glm::vec3(triangle.tangent[1]));
        tangents.push_back(glm::vec3(triangle.tangent[2]));
        triangle.tangent[reference.tvertex] = glm::vec4(glm::normalize(interpolate(p, p1, p2, p3, tangents.data())), triangle.tangent[0].w);

        triangle.color0[reference.tvertex] = interpolate(p, p1, p2, p3, triangle.color0);
    }
}

// Update triangle connections and edge error after a edge is collapsed

void update_triangles(int i0, Vertex& v, std::vector<int>& deleted, int& deleted_triangles)
{
    glm::vec3 p;
    for (int k = 0; k < v.tcount; ++k) {
        Ref& r = refs[v.tstart + k];
        Triangle& t = triangles[r.tid];
        if (t.deleted)
            continue;
        if (deleted[k]) {
            t.deleted = 1;
            deleted_triangles++;
            continue;
        }
        t.v[r.tvertex] = i0;
        t.dirty = 1;
        t.err[0] = calculate_error(t.v[0], t.v[1], p);
        t.err[1] = calculate_error(t.v[1], t.v[2], p);
        t.err[2] = calculate_error(t.v[2], t.v[0], p);
        t.err[3] = std::min(t.err[0], std::min(t.err[1], t.err[2]));
        refs.push_back(r);
    }
}

// compact triangles, compute edge error and build reference list

void update_mesh(int iteration)
{
    if (iteration > 0) // compact triangles
    {
        int dst = 0;
        loopi(0, triangles.size()) if (!triangles[i].deleted)
        {
            triangles[dst++] = triangles[i];
        }
        triangles.resize(dst);
    }
    //
    // Init Quadrics by Plane & Edge Errors
    //
    // required at the beginning ( iteration == 0 )
    // recomputing during the simplification is not required,
    // but mostly improves the result for closed meshes
    //
    if (iteration == 0) {
        loopi(0, vertices.size())
            vertices[i]
                .q
            = SymetricMatrix(0.0);

        loopi(0, triangles.size())
        {
            Triangle& t = triangles[i];
            glm::vec3 n, p[3];
            loopj(0, 3)
            {
                p[j] = vertices[t.v[j]].position;
            }
            n = glm::cross(p[1] - p[0], p[2] - p[0]);
            n = glm::normalize(n);
            t.n = n; // update triangle normal
            loopj(0, 3)
            {
                vertices[t.v[j]].q = vertices[t.v[j]].q + SymetricMatrix(n.x, n.y, n.z, -glm::dot(n, p[0]));
            }
        }
        loopi(0, triangles.size())
        {
            // Calc Edge Error
            Triangle& t = triangles[i];
            glm::vec3 p;
            loopj(0, 3) t.err[j] = calculate_error(t.v[j], t.v[(j + 1) % 3], p);
            t.err[3] = std::min(t.err[0], std::min(t.err[1], t.err[2]));
        }
    }

    // Init Reference ID list
    loopi(0, vertices.size())
    {
        vertices[i].tstart = 0;
        vertices[i].tcount = 0;
    }
    loopi(0, triangles.size())
    {
        Triangle& t = triangles[i];
        loopj(0, 3) vertices[t.v[j]].tcount++;
    }
    int tstart = 0;
    loopi(0, vertices.size())
    {
        Vertex& v = vertices[i];
        v.tstart = tstart;
        tstart += v.tcount;
        v.tcount = 0;
    }

    // Write References
    refs.resize(triangles.size() * 3);
    loopi(0, triangles.size())
    {
        Triangle& t = triangles[i];
        loopj(0, 3)
        {
            Vertex& v = vertices[t.v[j]];
            refs[v.tstart + v.tcount].tid = i;
            refs[v.tstart + v.tcount].tvertex = j;
            v.tcount++;
        }
    }

    // Identify boundary : vertices[].border=0,1
    if (iteration == 0) {
        std::vector<int> vcount, vids;

        loopi(0, vertices.size())
            vertices[i]
                .border
            = 0;

        loopi(0, vertices.size())
        {
            Vertex& v = vertices[i];
            vcount.clear();
            vids.clear();
            loopj(0, v.tcount)
            {
                int k = refs[v.tstart + j].tid;
                Triangle& t = triangles[k];

                for (int k = 0; k < 3; ++k) {
                    int ofs = 0, id = t.v[k];
                    while (ofs < vcount.size()) {
                        if (vids[ofs] == id)
                            break;
                        ofs++;
                    }
                    if (ofs == vcount.size()) {
                        vcount.push_back(1);
                        vids.push_back(id);
                    } else
                        vcount[ofs]++;
                }
            }
            loopj(0, vcount.size()) if (vcount[j] == 1)
                vertices[vids[j]]
                    .border
                = 1;
        }
    }
}

// Finally compact mesh before exiting

void compact_mesh()
{
    int dst = 0;
    loopi(0, vertices.size())
    {
        vertices[i].tcount = 0;
    }
    loopi(0, triangles.size()) if (!triangles[i].deleted)
    {
        Triangle& t = triangles[i];
        triangles[dst++] = t;
        loopj(0, 3) vertices[t.v[j]].tcount = 1;
    }
    triangles.resize(dst);
    dst = 0;
    loopi(0, vertices.size()) if (vertices[i].tcount)
    {
        vertices[i].tstart = dst;
        vertices[dst].position = vertices[i].position;
        dst++;
    }
    loopi(0, triangles.size())
    {
        Triangle& t = triangles[i];
        loopj(0, 3) t.v[j] = vertices[t.v[j]].tstart;
    }
    vertices.resize(dst);
}

// Error between vertex and Quadric

double vertex_error(SymetricMatrix q, double x, double y, double z)
{
    return q[0] * x * x + 2 * q[1] * x * y + 2 * q[2] * x * z + 2 * q[3] * x + q[4] * y * y
        + 2 * q[5] * y * z + 2 * q[6] * y + q[7] * z * z + 2 * q[8] * z + q[9];
}

// Error for one edge

double calculate_error(int id_v1, int id_v2, glm::vec3& p_result)
{
    // compute interpolated vertex

    SymetricMatrix q = vertices[id_v1].q + vertices[id_v2].q;
    bool border = vertices[id_v1].border & vertices[id_v2].border;
    double error = 0;
    double det = q.det(0, 1, 2, 1, 4, 5, 2, 5, 7);
    if (det != 0 && !border) {

        // q_delta is invertible
        p_result.x = -1 / det * (q.det(1, 2, 3, 4, 5, 6, 5, 7, 8)); // vx = A41/det(q_delta)
        p_result.y = 1 / det * (q.det(0, 2, 3, 1, 5, 6, 2, 7, 8)); // vy = A42/det(q_delta)
        p_result.z = -1 / det * (q.det(0, 1, 3, 1, 4, 6, 2, 5, 8)); // vz = A43/det(q_delta)

        error = vertex_error(q, p_result.x, p_result.y, p_result.z);
    } else {
        // det = 0 -> try to find best result
        glm::vec3 p1 = vertices[id_v1].position;
        glm::vec3 p2 = vertices[id_v2].position;
        glm::vec3 p3 = (p1 + p2) / 2.0f;
        double error1 = vertex_error(q, p1.x, p1.y, p1.z);
        double error2 = vertex_error(q, p2.x, p2.y, p2.z);
        double error3 = vertex_error(q, p3.x, p3.y, p3.z);
        error = std::min(error1, std::min(error2, error3));
        if (error1 == error)
            p_result = p1;
        if (error2 == error)
            p_result = p2;
        if (error3 == error)
            p_result = p3;
    }
    return error;
}

}

UX3D_END_IGNORE_WARNINGS
