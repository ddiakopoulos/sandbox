#ifndef procedural_mesh_h
#define procedural_mesh_h

#include "linalg_util.hpp"
#include "math_util.hpp"
#include "geometric.hpp"
#include "splines.hpp"
#include "geometry.hpp"
#include "algo_misc.hpp"
#include <assert.h>
#include <map>

namespace avl
{

    inline void remove_seams(const std::vector<float3> & vertices, std::vector<float3> & attribute)
    {
        std::map<float3, float3> smooth;
        for (size_t i = 0; i<vertices.size(); ++i) smooth[vertices[i]] += attribute[i];
        for (size_t i = 0; i<vertices.size(); ++i) attribute[i] = normalize(smooth[vertices[i]]); 
    }

    inline Geometry make_cube()
    {
        Geometry cube;
        
        const struct CubeVertex { float3 position, normal; float2 texCoord; } verts[] =
        {
            { { -1, -1, -1 }, { -1, 0, 0 }, { 0, 0 } }, { { -1, -1, +1 }, { -1, 0, 0 }, { 1, 0 } }, { { -1, +1, +1 }, { -1, 0, 0 }, { 1, 1 } }, { { -1, +1, -1 }, { -1, 0, 0 }, { 0, 1 } },
            { { +1, -1, +1 }, { +1, 0, 0 }, { 0, 0 } }, { { +1, -1, -1 }, { +1, 0, 0 }, { 1, 0 } }, { { +1, +1, -1 }, { +1, 0, 0 }, { 1, 1 } }, { { +1, +1, +1 }, { +1, 0, 0 }, { 0, 1 } },
            { { -1, -1, -1 }, { 0, -1, 0 }, { 0, 0 } }, { { +1, -1, -1 }, { 0, -1, 0 }, { 1, 0 } }, { { +1, -1, +1 }, { 0, -1, 0 }, { 1, 1 } }, { { -1, -1, +1 }, { 0, -1, 0 }, { 0, 1 } },
            { { +1, +1, -1 }, { 0, +1, 0 }, { 0, 0 } }, { { -1, +1, -1 }, { 0, +1, 0 }, { 1, 0 } }, { { -1, +1, +1 }, { 0, +1, 0 }, { 1, 1 } }, { { +1, +1, +1 }, { 0, +1, 0 }, { 0, 1 } },
            { { -1, -1, -1 }, { 0, 0, -1 }, { 0, 0 } }, { { -1, +1, -1 }, { 0, 0, -1 }, { 1, 0 } }, { { +1, +1, -1 }, { 0, 0, -1 }, { 1, 1 } }, { { +1, -1, -1 }, { 0, 0, -1 }, { 0, 1 } },
            { { -1, +1, +1 }, { 0, 0, +1 }, { 0, 0 } }, { { -1, -1, +1 }, { 0, 0, +1 }, { 1, 0 } }, { { +1, -1, +1 }, { 0, 0, +1 }, { 1, 1 } }, { { +1, +1, +1 }, { 0, 0, +1 }, { 0, 1 } },
        };
        
        std::vector<uint4> quads = { { 0, 1, 2, 3 }, { 4, 5, 6, 7 }, { 8, 9, 10, 11 }, { 12, 13, 14, 15 }, { 16, 17, 18, 19 }, { 20, 21, 22, 23 } };
        
        for(auto & q : quads)
        {
            cube.faces.push_back({q.x,q.y,q.z});
            cube.faces.push_back({q.x,q.z,q.w});
        }
        
        for (int i = 0; i < 24; ++i)
        {
            cube.vertices.push_back(verts[i].position);
            cube.normals.push_back(verts[i].normal);
            cube.texCoords.push_back(verts[i].texCoord);
        }
        
        cube.compute_bounds();
        cube.compute_normals(false);
        cube.compute_tangents();
        
        return cube;
    }
    
    inline Geometry make_sphere(float radius)
    {
        Geometry sphereGeom;

        uint32_t U = 32, V = 32;

        for (uint32_t ui = 0; ui < U; ++ui)
        {
            for (uint32_t vi = 0; vi < V; ++vi)
            {
                float u = float(ui) / (U - 1) * float(ANVIL_PI);
                float v = float(vi) / (V - 1) * 2 * float(ANVIL_PI);
                float3 normal = spherical_coords(u, v);
                sphereGeom.vertices.push_back({ normal * radius });
                sphereGeom.normals.push_back(normal);
            }
        }

        for (uint32_t ui = 0; ui < U; ++ui)
        {
            uint32_t un = (ui + 1) % U;
            for (uint32_t vi = 0; vi < V; ++vi)
            {
                uint32_t vn = (vi + 1) % V;
                sphereGeom.faces.push_back({ ui * V + vi, un * V + vi, un * V + vn });
                sphereGeom.faces.push_back({ ui * V + vi, un * V + vn, ui * V + vn });
            }
        }

        sphereGeom.compute_bounds();

        return sphereGeom;
    }

    inline Geometry make_hemisphere(const uint32_t num_rings = 33, const uint32_t num_sides = 32, float start_angle_rad = 0.f, const float end_angle_rad = ANVIL_PI / 2)
    {
        Geometry hemi;

        const uint32_t vertex_count = (num_sides + 1) * (num_rings + 1);
        hemi.vertices.resize(vertex_count);
        hemi.normals.resize(vertex_count);
        hemi.texCoords.resize(vertex_count);

        struct ArcVertex { float3 position, normal; };
        std::vector<ArcVertex> arcVerts(num_rings + 1);

        const float range = (end_angle_rad - start_angle_rad);

        for (uint32_t i = 0; i < arcVerts.size(); i++)
        {
            const float a = start_angle_rad + ((float)i / num_rings) * range;
            const float b = ((float)i / num_rings) * range;
            arcVerts[i].position = float3(0.f, std::sin(a), std::cos(b));
            arcVerts[i].normal = normalize(arcVerts[i].position);
        }

        for (uint32_t s = 0; s < num_sides + 1; s++)
        {
            const auto transform = make_rotation_matrix(make_rotation_quat_around_y(to_radians(360.0f * (float) s / num_sides)));

            for (uint32_t v = 0; v < arcVerts.size(); v++)
            {
                uint32_t idx = (num_rings + 1) * s + v;
                hemi.vertices[idx] = transform_coord(transform, arcVerts[v].position);
                hemi.normals[idx] = transform_vector(transform, arcVerts[v].normal);
            }
        }

        for (uint32_t s = 0; s < num_sides; s++)
        {
            const uint32_t v0 = (s + 0) * (num_rings + 1);
            const uint32_t v1 = (s + 1) * (num_rings + 1);

            for (uint32_t r = 0; r < num_rings; r++)
            {
                hemi.faces.emplace_back(v0 + r, v1 + r + 0, v0 + r + 1);
                hemi.faces.emplace_back(v1 + r, v1 + r + 1, v0 + r + 1);
            }
        }

        hemi.compute_bounds();

        return hemi;
    }

    inline Geometry make_cylinder(float radiusTop, float radiusBottom, float height, int radialSegments, int heightSegments, bool openEnded = false)
    {
        Geometry cylinderGeom;

        const float heightHalf = height / 2.f;

        std::vector<std::vector<uint32_t>> vertexRowArray;

        // Build up
        for (int y = 0; y <= heightSegments; y++)
        {
            std::vector<uint32_t> newRow;

            float v = (float)y / heightSegments;
            float radius = v * (radiusBottom - radiusTop) + radiusTop;

            // Build Around
            for (int x = 0; x <= radialSegments + 1; x++)
            {
                const float u = (float)x / (float)radialSegments;

                float3 vertex;
                vertex.x = radius * std::sin(u * ANVIL_TAU);
                vertex.y = -v * height + heightHalf;
                vertex.z = radius * std::cos(u * ANVIL_TAU);

                cylinderGeom.vertices.push_back(vertex);
                //cylinderGeom.normals.push_back(normalize(cross(-float3(0, 1, 0), vertex)));
                newRow.push_back((int)cylinderGeom.vertices.size() - 1);

                if (x == 0) std::cout << "Cyl: " << cylinderGeom.vertices.back() << std::endl;

            }

            vertexRowArray.push_back(newRow);
        }

        for (int x = 0; x < radialSegments; x++)
        {

            for (int y = 0; y < heightSegments; y++)
            {
                uint32_t v1 = vertexRowArray[y + 0][x + 0];
                uint32_t v2 = vertexRowArray[y + 1][x + 0];
                uint32_t v3 = vertexRowArray[y + 1][x + 1];
                uint32_t v4 = vertexRowArray[y + 0][x + 1];

                cylinderGeom.faces.emplace_back(v1, v2, v4);
                cylinderGeom.faces.emplace_back(v2, v3, v4);
            }
        }

        // Top
        if (!openEnded && radiusTop > 0)
        {
            cylinderGeom.vertices.push_back(float3(0, heightHalf, 0));

            for (int x = 0; x < radialSegments; x++)
            {

                uint32_t v1 = vertexRowArray[0][x + 0];
                uint32_t v2 = vertexRowArray[0][x + 1];
                uint32_t v3 = (int) cylinderGeom.vertices.size() - 1;

                cylinderGeom.faces.emplace_back(uint3(v1, v2, v3));
                cylinderGeom.normals.push_back(float3(0, 1, 0));
                cylinderGeom.normals.push_back(float3(0, 1, 0));
                cylinderGeom.normals.push_back(float3(0, 1, 0));
            }

        }

        // Bottom
        if (!openEnded && radiusBottom > 0)
        {
            cylinderGeom.vertices.push_back(float3(0, -heightHalf, 0));

            for (int x = 0; x < radialSegments; x++)
            {
                uint32_t v1 = vertexRowArray[heightSegments][x + 1];
                uint32_t v2 = vertexRowArray[heightSegments][x + 0];
                uint32_t v3 = (int)cylinderGeom.vertices.size() - 1;

                cylinderGeom.faces.emplace_back(uint3(v1, v2, v3));
                cylinderGeom.normals.push_back(float3(0, -1, 0));
                cylinderGeom.normals.push_back(float3(0, -1, 0));
                cylinderGeom.normals.push_back(float3(0, -1, 0));
            }

        }

        cylinderGeom.compute_normals();

        return cylinderGeom;
    }

    inline Geometry make_tapered_capsule()
    {
        const float height = 0.50f;
        const float3 hemi_bottom = float3(0, -height / 2.f, 0);
        const float3 hemi_top = float3(0, height / 2.f, 0);

        Geometry cylinder = make_cylinder(0.1, 0.2, height, 32, 32, true);
        Geometry top = make_hemisphere(32, 32, asin(0.25));
        Geometry bottom = make_hemisphere(32, 32, asin(0.25));

        auto top_xform = make_translation_matrix(hemi_top);
        auto bottom_xform = make_translation_matrix(hemi_bottom);

        top_xform = mul(top_xform, make_scaling_matrix(0.1));
        bottom_xform = mul(bottom_xform, make_scaling_matrix(0.2));
        bottom_xform = mul(bottom_xform, make_rotation_matrix({ 1, 0, 0 }, ANVIL_PI));

        for (auto & v : top.vertices) v = transform_coord(top_xform, v);
        for (auto & n : top.normals) n = transform_vector(top_xform, n);
        for (auto & v : bottom.vertices) v = transform_coord(bottom_xform, v);
        for (auto & n : bottom.normals) n = transform_vector(bottom_xform, n);

        auto caps = concatenate_geometry(top, bottom);
        auto r = concatenate_geometry(caps, cylinder);

        return r;
    }
    
    inline Geometry make_ring(float innerRadius = 2.0f, float outerRadius = 2.5f)
    {
        Geometry ringGeom;
        
        uint32_t thetaSegments = 8;
        uint32_t phiSegments = 2;
        
        float thetaStart = 0.0;
        float thetaLength = ANVIL_TAU;
        
        float radius = innerRadius;
        float radiusStep = ((outerRadius - innerRadius) / (float) phiSegments);
        
        //std::vector<float2> uvs;
        
        // Number of circles inside ring
        for (uint32_t i = 0; i <= phiSegments; i++)
        {
            float3 vertex;
            
            // Segments per cicle
            for (uint32_t o = 0; o <= thetaSegments; o++)
            {
                auto segment = thetaStart + (float) o / (float) thetaSegments * thetaLength;
                
                vertex.x = radius * cos(segment);
                vertex.y = radius * sin(segment);
                vertex.z = 0;
                
                ringGeom.vertices.push_back(vertex);
                ringGeom.texCoords.emplace_back((vertex.x / outerRadius + 1.0f) / 2.0f, (vertex.y / outerRadius + 1.0f) / 2.0f);
                
            }
            radius += radiusStep;
        }
        
        for (uint32_t i = 0; i < phiSegments; i++)
        {
            uint32_t thetaSegment = i * thetaSegments;
            
            for (uint32_t o = 0; o <= thetaSegments; o++)
            {
                uint32_t segment = o + thetaSegment;
                
                uint32_t v1 = segment + i;
                uint32_t v2 = segment + thetaSegments + i;
                uint32_t v3 = segment + thetaSegments + 1 + i;
                
                ringGeom.faces.emplace_back(v1, v2, v3); // front
                //ringGeom.texCoords.push_back(uvs[v1], uvs[v2], uvs[v3]);
                
                v1 = segment + i;
                v2 = segment + thetaSegments + 1 + i;
                v3 = segment + 1 + i;
                
                ringGeom.faces.emplace_back(v1, v2, v3);
                //ringGeom.texCoords.push_back(uvs[v1], uvs[v2], uvs[v3]);
                
            }
            
        }
        
        ringGeom.compute_normals();
        ringGeom.compute_tangents();
        
        return ringGeom;
    }
    
    inline Geometry make_3d_ring(float innerRadius = 1.0f, float outerRadius = 2.0f, float length = 1.0f)
    {
        Geometry ringGeom;
        
        uint32_t rs = 24; // radial segments
        uint32_t rs2 = rs * 2;
        
        // Inner Ring
        for (uint32_t i = 0; i < rs2; i++)
        {
            float angle = i * ANVIL_TAU / rs;
            float x = innerRadius * cos(angle);
            float y = innerRadius * sin(angle);
            float z = (i < rs) ? -(length * 0.5f) : (length * 0.5f);
            ringGeom.vertices.emplace_back(x, y, z);
        }
        
        for (uint32_t i = 0; i < rs; i++)
        {
            uint4 q = uint4(i, i + rs, (i + 1) % rs + rs, (i + 1) % rs);
            ringGeom.faces.push_back({q.x,q.y,q.z}); // faces point in
            ringGeom.faces.push_back({q.x,q.z,q.w});
        }
        
        // Outer Ring
        for (uint32_t i = 0; i < rs2; i++)
        {
            float angle = i * ANVIL_TAU / rs;
            float x = outerRadius * cos(angle);
            float y = outerRadius * sin(angle);
            float z = (i < rs)?  -(length * 0.5f) : (length * 0.5f);
            ringGeom.vertices.emplace_back(x, y, z);
        }
        
        for (uint32_t i = 0; i < rs; i++)
        {
            uint32_t b = (uint32_t) ringGeom.vertices.size() / 2;
            uint4 q = uint4(b + i, (b + i) + rs, ((b + i) + 1) % rs + (3 * rs), ((b + i) + 1) % rs + (rs * 2));
            ringGeom.faces.push_back({q.w,q.z,q.x}); // faces point out
            ringGeom.faces.push_back({q.z,q.y,q.x});
        }
        
        // Top + Bottom
        for (uint32_t i = 0; i < rs; i++)
        {
            uint32_t x = i + rs;
            uint4 q = uint4(i, (i) % rs + (2 * rs), (i + 1) % rs + (2 * rs), (i + 1) % rs); // -Z end
            uint4 q2 = uint4(x, (x) % (2 * rs) + (2 * rs), (i + 1) % rs + (3 * rs), ((i + 1) % rs) + rs); // +Z end
            ringGeom.faces.push_back({q.w,q.z,q.x});
            ringGeom.faces.push_back({q.z,q.y,q.x});
            ringGeom.faces.push_back({q2.x,q2.y,q2.z});
            ringGeom.faces.push_back({q2.x,q2.z,q2.w});
        }
        
        ringGeom.compute_normals();
        
        return ringGeom;
    }
    
    inline Geometry make_frustum(float aspectRatio = 1.33333f)
    {
        Geometry frustum;
        
        float h = 1 / aspectRatio;
        
        frustum.vertices = {
            {+0, +0,   +0}, {-1, +h, -1.0}, 
            {+0, +0,   +0}, {+1, +h, -1.0}, 
            {+0, +0,   +0}, {-1, -h, -1.0},
            {+0, +0,   +0}, {+1, -h, -1.0}, 
            {-1, +h, -1.0}, {+1, +h, -1.0}, 
            {+1, +h, -1.0}, {+1, -h, -1.0}, 
            {+1, -h, -1.0}, {-1, -h, -1.0}, 
            {-1, -h, -1.0}, {-1, +h, -1.0}
        };
        return frustum;
    }
    
    inline Geometry make_torus(uint32_t radial_segments = 24)
    {
        Geometry torus;
        
        for (uint32_t i = 0; i <= radial_segments; ++i)
        {
            auto a = make_rotation_quat_axis_angle({0,1,0}, (i % radial_segments) * ANVIL_TAU / radial_segments);
            for (uint32_t j = 0; j <= radial_segments; ++j)
            {
                auto b = make_rotation_quat_axis_angle({0,0,1}, (j % radial_segments) * ANVIL_TAU / radial_segments);
                torus.vertices.push_back(qrot(a, qrot(b, float3(1,0,0)) + float3(3,0,0)));
                torus.texCoords.push_back({i * 8.0f / radial_segments, j * 4.0f / radial_segments});
                if (i > 0 && j > 0)
                {
                    float4 q = {
                        float((i-1)* (radial_segments + 1)+(j-1)),
                        float(i*(radial_segments + 1) + (j-1)),
                        float(i*(radial_segments + 1) + j),
                        float((i-1)*(radial_segments+ 1) +j)
                    };
                    torus.faces.emplace_back(float3(q.x, q.y, q.z));
                    torus.faces.emplace_back(float3(q.x, q.z, q.w));
                }
            }
        }
        torus.compute_normals();
        torus.compute_tangents();
        return torus;
    }

    inline Geometry make_capsule(int segments, float radius, float length)
    {
        Geometry capsule;
        
        // Ensure odd
        segments = (segments + 1) &~1;
        
        int doubleSegments = segments * 2;
        float halfLength = length / 2;
        
        for (int j = 0; j < doubleSegments; ++j)
        {
            float ty = halfLength + radius;
            capsule.vertices.emplace_back(0.f, ty, 0.f);
            capsule.normals.emplace_back(0.f, 1.f, 0.f);
            capsule.texCoords.emplace_back((j+1) / float(segments), 0.f);
        }
        
        for (int i = 1; i < segments; ++i)
        {
            float r = sinf(i * ANVIL_PI / segments) * radius;
            float y = cosf(i * ANVIL_PI / segments);
            float ty = y * radius;
            
            if (i < segments/2)
                ty += halfLength;
            else
                ty -= halfLength;
            
            capsule.vertices.emplace_back(0.f, ty, -r);
            capsule.normals.push_back(safe_normalize(float3(0.f, y, -1.f)));
            capsule.texCoords.emplace_back(0.f, i / float(segments));
            
            for (int j = 1; j < doubleSegments; ++j)
            {
                float x = sinf(j * ANVIL_TAU / doubleSegments);
                float z = -cosf(j * ANVIL_TAU / doubleSegments);
                float ty = y * radius;
                
                if (i < segments/2)
                    ty += halfLength;
                else
                    ty -= halfLength;
                
                capsule.vertices.emplace_back(x * r, ty , z * r);
                capsule.normals.push_back(safe_normalize(float3(x, y, z)));
                capsule.texCoords.emplace_back(j / float(segments), i / float(segments));
            }
            
            capsule.vertices.emplace_back(0.f, ty, -r);
            capsule.normals.push_back(safe_normalize(float3(0.f, y, -1.f)));
            capsule.texCoords.emplace_back(2.f, i / float(segments));
        }
        
        for (int j = 0; j < doubleSegments; ++j)
        {
            float ty = -halfLength - radius;
            capsule.vertices.emplace_back(0.f, ty, 0.f);
            capsule.normals.push_back(safe_normalize(float3(0.f, -1.f, 0.f)));
            capsule.texCoords.emplace_back((j+1) / float(segments), 1.f);
        }
        
        int v = 0;
        for (int j = 0; j < doubleSegments; ++j)
        {
            capsule.faces.emplace_back(v, v + doubleSegments + 1, v + doubleSegments);
            ++v;
        }
        
        for (int i = 1; i < segments - 1; ++i)
        {
            for(int j=0; j < doubleSegments; ++j)
            {
                capsule.faces.emplace_back(v, v + 1, v + doubleSegments + 2);
                capsule.faces.emplace_back(v, v + doubleSegments + 2,v + doubleSegments + 1);
                ++v;
            }
            ++v;
        }
        
        for (int j = 0; j < doubleSegments; ++j)
        {
            capsule.faces.emplace_back(v, v + 1, v + doubleSegments + 1);
            ++v;
        }
        return capsule;
    }

    inline Geometry make_plane(float width, float height, uint32_t nw, uint32_t nh, bool withBackface = false)
    {
        Geometry plane;
        uint32_t indexOffset = 0;
        
        float rw = 1.f / width;
        float rh = 1.f / height;
        float ow = width / nw;
        float oh = height / nh;
        
        float ou = ow * rw;
        float ov = oh * rh;
        
        for (float w = -width / 2.0; w < width / 2.0; w += ow)
        {
            for (float h = -height / 2.0; h < height / 2.0; h += oh)
            {
                float u = (w + width / 2.0) * rw;
                float v = (h + height / 2.0) * rh;
                
                plane.vertices.emplace_back(w, h + oh, 0.f);
                plane.vertices.emplace_back(w, h, 0.f);
                plane.vertices.emplace_back(w + ow, h, 0.f);
                plane.vertices.emplace_back(w + ow, h + oh, 0.f);
                
                plane.texCoords.emplace_back(u, v + ov);
                plane.texCoords.emplace_back(u, v);
                plane.texCoords.emplace_back(u + ou, v);
                plane.texCoords.emplace_back(u + ou, v + ov);

                plane.faces.push_back({indexOffset + 0, indexOffset + 1, indexOffset + 2});
                plane.faces.push_back({indexOffset + 0, indexOffset + 2, indexOffset + 3});

                if (withBackface)
                {
                    plane.faces.push_back({ indexOffset + 2, indexOffset + 1, indexOffset + 0 });
                    plane.faces.push_back({ indexOffset + 3, indexOffset + 2, indexOffset + 0 });
                }
                
                indexOffset += 4;
            }
        }
        
        plane.compute_normals(false);
        plane.compute_tangents();
        plane.compute_bounds();
        
        return plane;
    }
    
    inline Geometry make_curved_plane()
    {
        Geometry plane;
        
        auto curve = BezierCurve(float3(0.0f, 0.0f, 0.0f), float3(0.667f, 0.25f, 0.0f), float3(1.33f, 0.25f, 0.0f), float3(2.0f, 0.0f, 0.0f));
        
        const int numSegments = curve.num_steps();
        const int numSlices = numSegments + 1;
        const int numVerts = 2 * numSlices;
        
        plane.vertices.resize(numVerts);
        plane.normals.resize(numVerts);
        plane.texCoords.resize(numVerts);
        
        for (int i = 0; i <= numSegments; i++)
        {
            float t = (float)i / (float)numSegments;
            float3 point = curve.point(t);
            
            std::cout << point << std::endl;
            
            float3 tang = normalize(curve.derivative(t));
            float3 norm = float3(0.0f, 1.0f, 0.0f);
            float3 biNorm = cross(tang, norm);
            
            //std::cout << biNorm.x << ", " << biNorm.y << ", " << biNorm.z << std::endl;
            
            int index = i * 2; // Slice idx
            
            plane.vertices[index] = point + float3(0, 0, 1); //biNorm;
            plane.vertices[index + 1] = point - float3(0, 0, 1);// biNorm;
            
            plane.normals[index] = norm;
            plane.normals[index + 1] = norm;
            
            plane.texCoords[index] = float2(t, 0.0f);
            plane.texCoords[index + 1] = float2(t, 1.0f);
        }
        
        // Set up indices
        for (int i = 0; i < numSegments; i++)
        {
            int vIndex = i * 2; // Starting vertex index of this segment
            
            auto f1 = uint3(vIndex + 0, vIndex + 1, vIndex + 2);
            auto f2 = uint3(vIndex + 1, vIndex + 3, vIndex + 2);
            
            plane.faces.push_back(f1);
            plane.faces.push_back(f2);
        }
        
        plane.compute_tangents();
        
        return plane;
    }
    
    inline Geometry make_axis()
    {
        Geometry axis;
        
        axis.vertices.emplace_back(0.f, 0.f, 0.f);
        axis.vertices.emplace_back(1.f, 0.f, 0.f);
        axis.vertices.emplace_back(0.f, 0.f, 0.f);
        axis.vertices.emplace_back(0.f, 1.f, 0.f);
        axis.vertices.emplace_back(0.f, 0.f, 0.f);
        axis.vertices.emplace_back(0.f, 0.f, 1.f);

        axis.colors.emplace_back(0.f, 0.f, 1.f, 1.f);
        axis.colors.emplace_back(0.f, 0.f, 1.f, 1.f);
        axis.colors.emplace_back(0.f, 1.f, 0.f, 1.f);
        axis.colors.emplace_back(0.f, 1.f, 0.f, 1.f);
        axis.colors.emplace_back(1.f, 0.f, 0.f, 1.f);
        axis.colors.emplace_back(1.f, 0.f, 0.f, 1.f);

        axis.normals.emplace_back(0.f, 0.f, 1.f);
        axis.normals.emplace_back(0.f, 0.f, 1.f);
        axis.normals.emplace_back(0.f, 1.f, 0.f);
        axis.normals.emplace_back(0.f, 1.f, 0.f);
        axis.normals.emplace_back(1.f, 0.f, 0.f);
        axis.normals.emplace_back(1.f, 0.f, 0.f);

        return axis;
    }
    
    inline Geometry make_spiral(float resolution = 512.0f, float freq = 128.f)
    {
        Geometry spiral;
        float off = 1.0 / resolution;
        for (float i = 0.0; i < 1.0 + off; i += off)
        {
            float s = cos(i * 2.0 * ANVIL_PI + ANVIL_PI) * 0.5f + 0.5f;
            spiral.vertices.push_back({cosf(i * ANVIL_PI * freq) * s, i, sinf(i * ANVIL_PI * freq) * s});
        }
        return spiral;
    }

    inline Geometry make_icosahedron()
    {
        Geometry icosahedron;
        const float t = (1.0f + sqrtf(5.0f)) / 2.0f;
        
        icosahedron.vertices = {
            {-1.0, t, 0.0}, {1.0, t, 0.0}, {-1.0, -t, 0.0}, {1.0, -t, 0.0},
            {0.0, -1.0, t}, {0.0, 1.0, t}, {0.0, -1.0, -t}, {0.0, 1.0, -t},
            {t, 0.0, -1.0}, {t, 0.0, 1.0}, {-t, 0.0, -1.0}, {-t, 0.0, 1.0}
        };
        
        icosahedron.faces = {
            {0, 11, 5}, {0, 5, 1}, {0, 1, 7}, {0, 7, 10}, {0, 10, 11},
            {1, 5, 9}, {5, 11, 4}, {11, 10, 2}, {10, 7, 6}, {7, 1, 8},
            {3, 9, 4}, {3, 4, 2}, {3, 2, 6}, {3, 6, 8}, {3, 8, 9},
            {4, 9, 5}, {2, 4, 11}, {6, 2, 10}, {8, 6, 7}, {9, 8, 1}
        };
        
        icosahedron.compute_normals();
        return icosahedron;
    }
    
    inline Geometry make_octohedron()
    {
        Geometry octohedron;
        octohedron.vertices = {
            {1.0, 0.0, 0.0}, {-1.0, 0.0, 0.0},
            {0.0, 1.0, 0.0}, {0.0, -1.0, 0.0},
            {0.0, 0.0, 1.0}, {0.0, 0.0, -1.0}
        };
        octohedron.faces = {{0, 2, 4}, {0, 4, 3}, {0, 3, 5}, {0, 5, 2}, {1, 2, 5}, {1, 5, 3}, {1, 3, 4}, {1, 4, 2}};
        octohedron.compute_normals();
        return octohedron;
    }
    
    inline Geometry make_tetrahedron()
    {
        Geometry tetrahedron;
        tetrahedron.vertices = {
            {1.0, 1.0, 1.0}, {-1.0, -1.0, 1.0},
            {-1.0, 1.0, -1.0}, {1.0, -1.0, -1.0}
        };
        tetrahedron.faces = {{2, 1, 0},{0, 3, 2},{1, 3, 0},{2, 3, 1}};
        tetrahedron.compute_normals();
        return tetrahedron;
    }

    inline Geometry make_supershape_3d(const int segments, const float m, const float n1, const float n2, const float n3, const float a = 1.0, const float b = 1.0)
    {
        Geometry shape;

        SuperFormula f1(m, n1, n2, n3, a, b);
        SuperFormula f2(m, n1, n2, n3, a, b);

        float theta = -ANVIL_PI;
        float lon_inc = ANVIL_TAU / segments;
        float lat_inc = ANVIL_PI / segments;

        // Longitude
        for (int i = 0; i < segments + 1; ++i)
        {
            const float r1 = f1(theta);
            float phi = -ANVIL_PI / 2.0f; // reset phi 

            // Latitude
            for (int j = 0; j < segments + 1; ++j)
            {
                const float r2 = f2(phi);
                const float radius = r1 * r2; // spherical product
                const float x = radius * cos(theta) * cos(phi);
                const float y = radius * sin(theta) * cos(phi);
                const float z = r2 * sin(phi);
                shape.vertices.emplace_back(x, y, z);
                phi += lat_inc;
            }

            theta += lon_inc;
        }

        std::vector<uint4> quads;
        int latIdx = 0;
        for (int i = 0; i < segments * (segments + 1); ++i)
        {
            if (latIdx < segments)
            {
                uint32_t a = i;
                uint32_t b = i + 1;
                uint32_t c = i + segments + 1 + 1;
                uint32_t d = i + segments + 1;
                quads.push_back(uint4(a, b, c, d));
                latIdx++;
            }
            else latIdx = 0;
        }

        for (auto & q : quads)
        {
            shape.faces.push_back({ q.w,q.z,q.x });
            shape.faces.push_back({ q.z,q.y,q.x });
        }

        shape.compute_normals(true);
        return shape;
    }
    
}

#endif // procedural_mesh_h
