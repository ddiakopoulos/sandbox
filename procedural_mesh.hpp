#ifndef procedural_mesh_h
#define procedural_mesh_h

#include "linalg_util.hpp"
#include "math_util.hpp"
#include "geometric.hpp"
#include <assert.h>
#include "GL_API.hpp"

namespace avl
{
    
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
        
        cube.compute_normals(false);
        cube.compute_tangents();
        
        return cube;
    }
    
    inline GlMesh make_cube_mesh()
    {
        return make_mesh_from_geometry(make_cube());
    }
    
    inline Geometry make_sphere(float radius)
    {
        Geometry sphereGeom;
        
        int U = 32, V = 32;
        
        for (int ui = 0; ui < U; ++ui)
        {
            for (int vi = 0; vi < V; ++vi)
            {
                float u = float(ui) / (U - 1) * ANVIL_PI;
                float v = float(vi) / (V - 1) * 2 * ANVIL_PI;
                float3 normal = spherical(u, v);
                sphereGeom.vertices.push_back({normal * radius});
                sphereGeom.normals.push_back(normal);
            }
        }
        
        for (uint32_t ui = 0; ui < U; ++ui)
        {
            uint32_t un = (ui + 1) % U;
            for (uint32_t vi = 0; vi < V; ++vi)
            {
                uint32_t vn = (vi + 1) % V;
                sphereGeom.faces.push_back({ui * V + vi, un * V + vi, un * V + vn});
                sphereGeom.faces.push_back({ui * V + vi, un * V + vn, ui * V + vn});
            }
        }
        return sphereGeom;
    }
    
    inline GlMesh make_sphere_mesh(float radius)
    {
        return make_mesh_from_geometry(make_sphere(radius));
    }
    
    inline Geometry make_cylinder(float radiusTop, float radiusBottom, float height, int radialSegments, int heightSegments, bool openEnded = false)
    {
        Geometry cylinderGeom;
        
        float heightHalf = height / 2.f;
        
        std::vector<std::vector<uint32_t>> vertexRowArray;
        
        for (int y = 0; y <= heightSegments; y++)
        {
            std::vector<uint32_t> newRow;
            
            float v = (float) y / heightSegments;
            float radius = v * (radiusBottom - radiusTop) + radiusTop;
            
            for (int x = 0; x <= radialSegments; x++)
            {
                float u = (float) x / (float) radialSegments;
                
                float3 vertex;
                
                vertex.x = radius * sin(u * ANVIL_TAU);
                vertex.y = -v * height + heightHalf;
                vertex.z = radius * cos(u * ANVIL_TAU);
                
                cylinderGeom.vertices.push_back(vertex);
                newRow.push_back((int) cylinderGeom.vertices.size() - 1);
            }
            
            vertexRowArray.push_back(newRow);
        }
        
        float tanTheta = (radiusBottom - radiusTop) / height;
        
        float3 na, nb;
        
        for (int x = 0; x < radialSegments; x++)
        {
            if (radiusTop != 0)
            {
                na = cylinderGeom.vertices[vertexRowArray[0][x]];
                nb = cylinderGeom.vertices[vertexRowArray[0][x + 1]];
            }
            else
            {
                na = cylinderGeom.vertices[vertexRowArray[1][x]];
                nb = cylinderGeom.vertices[vertexRowArray[1][x + 1]];
            }
            
            na.y = sqrt(na.x * na.x + na.z * na.z) * tanTheta;
            nb.y = sqrt(nb.x * nb.x + nb.z * nb.z) * tanTheta;
            
            na = safe_normalize(na);
            nb = safe_normalize(nb);
            
            for (int y = 0; y < heightSegments; y++)
            {
                uint32_t v1 = vertexRowArray[y][x];
                uint32_t v2 = vertexRowArray[y + 1][x];
                uint32_t v3 = vertexRowArray[y + 1][x + 1];
                uint32_t v4 = vertexRowArray[y][x + 1];
                
                float3 n1 = na;
                float3 n2 = na;
                float3 n3 = nb;
                float3 n4 = nb;
                
                cylinderGeom.faces.emplace_back(v1, v2, v4);
                cylinderGeom.normals.push_back(n1);
                cylinderGeom.normals.push_back(n2);
                cylinderGeom.normals.push_back(n3);
                
                
                cylinderGeom.faces.emplace_back(v2, v3, v4);
                cylinderGeom.normals.push_back(n2);
                cylinderGeom.normals.push_back(n3);
                cylinderGeom.normals.push_back(n4);
            }
        }
        
        // Top
        if (!openEnded && radiusTop > 0)
        {
            cylinderGeom.vertices.push_back(float3(0, heightHalf, 0));
            
            for (int x = 0; x < radialSegments; x++)
            {
                
                uint32_t v1 = vertexRowArray[0][x];
                uint32_t v2 = vertexRowArray[0][x + 1];
                uint32_t v3 = (int) cylinderGeom.vertices.size() - 1;
                
                float3 n1 = float3(0, 1, 0);
                float3 n2 = float3(0, 1, 0);
                float3 n3 = float3(0, 1, 0);
                
                cylinderGeom.faces.emplace_back(uint3(v1, v2, v3));
                cylinderGeom.normals.push_back(n1);
                cylinderGeom.normals.push_back(n2);
                cylinderGeom.normals.push_back(n3);
            }
            
        }
        
        // Bottom
        if (!openEnded && radiusBottom > 0)
        {
            cylinderGeom.vertices.push_back(float3(0, -heightHalf, 0));
            
            for (int x = 0; x < radialSegments; x++)
            {
                uint32_t v1 = vertexRowArray[heightSegments][x + 1];
                uint32_t v2 = vertexRowArray[heightSegments][x];
                uint32_t v3 = (int) cylinderGeom.vertices.size() - 1;
                
                float3 n1 = float3(0, -1, 0);
                float3 n2 = float3(0, -1, 0);
                float3 n3 = float3(0, -1, 0);
                
                cylinderGeom.faces.emplace_back(uint3(v1, v2, v3));
                cylinderGeom.normals.push_back(n1);
                cylinderGeom.normals.push_back(n2);
                cylinderGeom.normals.push_back(n3);
            }
            
        }
        
        //cylinderGeom.compute_normals();
        
        return cylinderGeom;
    }
    
    inline GlMesh make_cylinder_mesh(float radiusTop, float radiusBottom, float height, int radialSegments, int heightSegments, bool openEnded = false)
    {
        return make_mesh_from_geometry(make_cylinder(radiusTop, radiusBottom, height, radialSegments, heightSegments, openEnded));
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
    
    inline GlMesh make_ring_mesh(float innerRadius = 1.0f, float outerRadius = 2.0f)
    {
        return make_mesh_from_geometry(make_ring(innerRadius, outerRadius));
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
        for (int i = 0; i < rs; i++)
        {
            int x = i + rs;
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
    
    inline GlMesh make_3d_ring_mesh(float innerRadius = 1.0f, float outerRadius = 2.0f, float length = 1.0f)
    {
        return make_mesh_from_geometry(make_3d_ring(innerRadius, outerRadius, length));
    }
    
    inline Geometry make_frustum(float aspectRatio = 1.33333f)
    {
        Geometry frustum;
        
        float h = 1 / aspectRatio;
        
        frustum.vertices = {
            {+0, +0, +0}, {-1, +h, -1.5}, {+0, +0, +0},
            {+1, +h, -1.5}, {+0, +0, +0}, {-1, -h, -1.5},
            {+0, +0, +0}, {+1, -h, -1.5}, {-1, +h, -1.5},
            {+1, +h, -1.5}, {+1, +h, -1.5}, {+1, -h, -1.5},
            {+1, -h, -1.5}, {-1, -h, -1.5}, {-1, -h, -1.5},
            {-1, +h, -1.5}
        };
        frustum.compute_normals();
        return frustum;
    }
    
    inline GlMesh make_frustum_mesh(float aspectRatio = 1.33333f)
    {
        auto frustumMesh = make_mesh_from_geometry(make_frustum(aspectRatio));
        frustumMesh.set_non_indexed(GL_LINES);
        return frustumMesh;
    }
    
    inline Geometry make_torus(int radial_segments = 24)
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
                    torus.faces.emplace_back(q.x, q.y, q.z);
                    torus.faces.emplace_back(q.x, q.z, q.w);
                }
            }
        }
        torus.compute_normals();
        torus.compute_tangents();
        return torus;
    }
    
    inline GlMesh make_torus_mesh(int radial_segments = 8)
    {
        return make_mesh_from_geometry(make_torus(radial_segments));
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
            capsule.vertices.emplace_back(0, ty, 0);
            capsule.normals.emplace_back(0, 1, 0);
            capsule.texCoords.emplace_back((j+1) / float(segments), 0);
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
            
            capsule.vertices.emplace_back(0, ty, -r);
            capsule.normals.push_back(safe_normalize(float3(0, y, -1)));
            capsule.texCoords.emplace_back(0, i / float(segments));
            
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
            
            capsule.vertices.emplace_back(0, ty, -r);
            capsule.normals.push_back(safe_normalize(float3(0, y, -1)));
            capsule.texCoords.emplace_back(2, i / float(segments));
        }
        
        for (int j = 0; j < doubleSegments; ++j)
        {
            float ty = -halfLength - radius;
            capsule.vertices.emplace_back(0, ty, 0);
            capsule.normals.push_back(safe_normalize(float3(0, -1, 0)));
            capsule.texCoords.emplace_back((j+1) / float(segments), 1);
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
    
    inline GlMesh make_capsule_mesh(int segments, float radius, float length)
    {
        return make_mesh_from_geometry(make_capsule(segments, radius, length));
    }
    
    inline Geometry make_plane(float width, float height, uint32_t nw, uint32_t nh)
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
                
                plane.vertices.emplace_back(w, h + oh, 0);
                plane.vertices.emplace_back(w, h, 0);
                plane.vertices.emplace_back(w + ow, h, 0);
                plane.vertices.emplace_back(w + ow, h + oh, 0);
                
                plane.texCoords.emplace_back(u, v + ov);
                plane.texCoords.emplace_back(u, v);
                plane.texCoords.emplace_back(u + ou, v);
                plane.texCoords.emplace_back(u + ou, v + ov);
                
                // Front faces
                plane.faces.push_back({indexOffset + 0, indexOffset + 1, indexOffset + 2});
                plane.faces.push_back({indexOffset + 0, indexOffset + 2, indexOffset + 3});
                
                // Back faces
                //plane.faces.push_back({indexOffset + 2, indexOffset + 1, indexOffset + 0});
                //plane.faces.push_back({indexOffset + 3, indexOffset + 2, indexOffset + 0});
                
                indexOffset += 4;
            }
        }
        
        plane.compute_normals(false);
        plane.compute_tangents();
        
        return plane;
    }
    
    inline GlMesh make_plane_mesh(float width, float height, uint32_t nw, uint32_t nh)
    {
        return make_mesh_from_geometry(make_plane(width, height, nw, nh));
    }
    
    
    class BezierCurve
    {
        
        void calculate_length()
        {
            arcLengths.resize(num_steps());
            arcLengths[0] = 0.0f;
            
            float3 start = p0;
            for (int i = 1; i < num_steps(); i++)
            {
                float t = (float)i / (float)(num_steps() - 1);
                float3 end = point(t);
                float length = distance(start, end);
                arcLengths[i] = arcLengths[i - 1] + length;
                
                start = end;
            }
        }
        
        float3 p0, p1, p2, p3;
        std::vector<float> arcLengths;
        
    public:
        
        BezierCurve(float3 p0, float3 p1, float3 p2, float3 p3)
        {
            set_control_points(p0, p1, p2, p3);
        }
        
        void set_control_points(float3 p0, float3 p1, float3 p2, float3 p3)
        {
            this->p0 = p0;
            this->p1 = p1;
            this->p2 = p2;
            this->p3 = p3;
            
            calculate_length();
        }
        
        float num_steps() const
        {
            return 32;
        }
        
        float3 point(const float t) const
        {
            float t2 = t * t;
            float t3 = t2 * t;
            float tt1 = 1.0f - t;
            float tt2 = tt1 * tt1;
            float tt3 = tt2 * tt1;
            return (tt3 * p0) + (3.0f * t * tt2 * p1) + (3.0f * tt1 * t2 * p2) + (t3 * p3);
        }
        
        float3 derivative(const float t) const
        {
            float t2 = t * t;
            float tt1 = 1.0f - t;
            float tt2 = tt1 * tt1;
            return (-3.0f * tt2 * p0) + ((3.0f * tt2 - 6.0f * t * tt1) * p1) + ((6.0f * t * tt1 - 3.0f * t2) * p2) + (3.0f * t2 * p3);
        }
        
        float3 derivative2(const float t) const
        {
            return 6.0f * (1.0f - t) * (p2 - 2.0f * p1 + p0) + 6.0f * t * (p3 - 2.0f * p2 + p1);
        }
        
        float curvature(const float t) const
        {
            float3 deriv = derivative(t);
            float3 deriv2 = derivative2(t);
            return linalg::length(cross(deriv, deriv2)) / powf(linalg::length(deriv), 3.0f);
        }
        
        float max_curvature() const
        {
            float max = std::numeric_limits<float>::min();
            for (int i = 0; i < num_steps(); i++)
            {
                float t = (float) i / (float)(num_steps() - 1);
                float c = curvature(t);
                if (c > max) max = c;
            }
            
            return max;
        }
        
        float length() const
        {
            return arcLengths[arcLengths.size() - 1];
        }
        
        float get_length_parameter(float t)
        {
            float targetLength = t * arcLengths[arcLengths.size() - 1];
            
            // Find largest arc length that is less than the target length
            size_t first = 0;
            size_t last = arcLengths.size() - 1;
            while (first <= last)
            {
                auto mid = (first + last) / 2;
                if (arcLengths[mid] > targetLength) last = mid - 1;
                else first = mid + 1;
            }
            
            size_t index = first - 1;
            if (arcLengths[index] == targetLength) // No need to interpolate
                return index / (float)(arcLengths.size() - 1);
            
            // Begin interpolation
            float start = arcLengths[index];
            float end = arcLengths[index + 1];
            float length = end - start;
            float fraction = (targetLength - start) / length;
            return (index + fraction) / (float)(arcLengths.size() - 1);
        }
        
    };
    
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
    
    inline GlMesh make_curved_plane_mesh()
    {
        return make_mesh_from_geometry(make_curved_plane());
    }
    
    inline Geometry make_axis()
    {
        Geometry axis;
        
        axis.vertices.emplace_back(0.0, 0.0, 0.0);
        axis.vertices.emplace_back(1.0, 0.0, 0.0);
        axis.vertices.emplace_back(0.0, 0.0, 0.0);
        axis.vertices.emplace_back(0.0, 1.0, 0.0);
        axis.vertices.emplace_back(0.0, 0.0, 0.0);
        axis.vertices.emplace_back(0.0, 0.0, 1.0);
        
        // X = Blue, Y = Green, Z = Red
        axis.colors.emplace_back(0.0, 0.0, 1.0, 1.0);
        axis.colors.emplace_back(0.0, 0.0, 1.0, 1.0);
        axis.colors.emplace_back(0.0, 1.0, 0.0, 1.0);
        axis.colors.emplace_back(0.0, 1.0, 0.0, 1.0);
        axis.colors.emplace_back(1.0, 0.0, 0.0, 1.0);
        axis.colors.emplace_back(1.0, 0.0, 0.0, 1.0);
        
        return axis;
    }
    
    inline GlMesh make_axis_mesh()
    {
        auto axisMesh = make_mesh_from_geometry(make_axis());
        axisMesh.set_non_indexed(GL_LINES);
        return axisMesh;
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
    
    inline GlMesh make_spiral_mesh(float resolution = 512.0f, float freq = 128.f)
    {
        assert(freq < resolution);
        auto sprialMesh = make_mesh_from_geometry(make_spiral());
        sprialMesh.set_non_indexed(GL_LINE_STRIP);
        return sprialMesh;
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
    
    inline GlMesh make_icosahedron_mesh()
    {
        return make_mesh_from_geometry(make_icosahedron());
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
    
    inline GlMesh make_octohedron_mesh()
    {
        return make_mesh_from_geometry(make_octohedron());
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
    
    inline GlMesh make_tetrahedron_mesh()
    {
        return make_mesh_from_geometry(make_tetrahedron());
    }
    
    inline GlMesh make_fullscreen_quad()
    {
        Geometry g;
        g.vertices = { {-1.0f, -1.0f, 0.0f}, {1.0f, -1.0f, 0.0f}, {-1.0f, 1.0f, 0.0f}, {-1.0f, 1.0f, 0.0f}, {1.0f, -1.0f, 0.0f}, {1.0f, 1.0f, 0.0f} };
        g.faces = {{0, 1, 2}, {3, 4, 5}};
        //g.texCoords = {{0, 0}, {1, 0}, {0, 1}, {0, 1}, {1, 0}, {1, 1}};
        return make_mesh_from_geometry(g);
    }
    
}

#endif // procedural_mesh_h
