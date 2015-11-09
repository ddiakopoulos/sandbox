#ifndef procedural_mesh_h
#define procedural_mesh_h

#include "linear_algebra.hpp"
#include "math_util.hpp"
#include "geometric.hpp"
#include "GlMesh.hpp"

inline gfx::GlMesh make_sphere_mesh(float radius)
{
    Geometry sphereGeom;
    
    int U = 32, V = 32;
    
    for (int ui = 0; ui < U; ++ui)
    {
        for (int vi = 0; vi < V; ++vi)
        {
            float u = float(ui) / (U - 1) * ANVIL_PI;
            float v = float(vi) / (V - 1) * 2 * ANVIL_PI;
            math::float3 normal = math::spherical(u, v);
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
    
    return make_mesh_from_geometry(sphereGeom);
}

inline gfx::GlMesh make_cube_mesh()
{
    Geometry cube;
 
    cube.vertices = {
        { -1, -1, -1 }, { -1, -1, +1 }, { -1, +1, +1 }, { -1, +1, -1 },
        { +1, -1, +1 }, { +1, -1, -1 }, { +1, +1, -1 }, { +1, +1, +1 },
        { -1, -1, -1 }, { +1, -1, -1 }, { +1, -1, +1 }, { -1, -1, +1 },
        { +1, +1, -1 }, { -1, +1, -1 }, { -1, +1, +1 }, { +1, +1, +1 },
        { -1, -1, -1 }, { -1, +1, -1 }, { +1, +1, -1 }, { +1, -1, -1 },
        { -1, +1, +1 }, { -1, -1, +1 }, { +1, -1, +1 }, { +1, +1, +1 }
    };
        
    cube.texCoords = {
        {0, 0}, {1, 0}, {1, 1}, {0, 0}, {1, 1}, {0, 1},
        {0, 0}, {1, 0}, {1, 1}, {0, 0}, {1, 1}, {0, 1},
        {0, 0}, {1, 0}, {1, 1}, {0, 0}, {1, 1}, {0, 1},
        {0, 0}, {1, 0}, {1, 1}, {0, 0}, {1, 1}, {0, 1}
    };

    cube.faces = {
        {0,  1,  2 }, {0,  2,  3 },
        {4,  5,  6 }, {4,  6,  7 },
        {8,  9,  10}, {8,  10, 11},
        {12, 13, 14}, {12, 14, 15},
        {16, 17, 18}, {16, 18, 19},
        {20, 21, 22}, {20, 22, 23}
    };
    
    cube.compute_normals();
    cube.compute_tangents();
    
    return make_mesh_from_geometry(cube);
}


inline gfx::GlMesh make_frustum_mesh(float aspectRatio = 1.33333f)
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
    auto frustumMesh = make_mesh_from_geometry(frustum);
    frustumMesh.set_non_indexed(GL_LINES);
    return frustumMesh;
}

inline gfx::GlMesh make_torus_mesh(int radial_segments = 8)
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
    return make_mesh_from_geometry(torus);
}

inline gfx::GlMesh make_capsule_mesh(int segments, float radius, float length)
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
        capsule.normals.push_back(normalize(float3(0, y, -1)));
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
            capsule.normals.push_back(normalize(float3(x, y, z)));
            capsule.texCoords.emplace_back(j / float(segments), i / float(segments));
        }
        
        capsule.vertices.emplace_back(0, ty, -r);
        capsule.normals.push_back(normalize(float3(0, y, -1)));
        capsule.texCoords.emplace_back(2, i / float(segments));
    }
    
    for (int j = 0; j < doubleSegments; ++j)
    {
        float ty = -halfLength - radius;
        capsule.vertices.emplace_back(0, ty, 0);
        capsule.normals.push_back(normalize(float3(0, -1, 0)));
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
    return make_mesh_from_geometry(capsule);
}

inline gfx::GlMesh make_plane_mesh(float width, float height, uint nw, uint nh)
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
            plane.faces.push_back({indexOffset + 2, indexOffset + 1, indexOffset + 0});
            plane.faces.push_back({indexOffset + 3, indexOffset + 2, indexOffset + 0});
            
            indexOffset += 4;
        }
    }
    
    plane.compute_normals();
    plane.compute_tangents();
    
    return make_mesh_from_geometry(plane);
}

inline gfx::GlMesh make_axis_mesh()
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
    
    auto axisMesh = make_mesh_from_geometry(axis);
    axisMesh.set_non_indexed(GL_LINES);
    return axisMesh;
}

inline gfx::GlMesh make_spiral_mesh(float resolution = 512.0f, float freq = 128.f)
{
    assert(freq < resolution);
    Geometry spiral;
    float off = 1.0 / resolution;
    for (float i = 0.0; i < 1.0 + off; i += off)
    {
        float s = cos(i * 2.0 * ANVIL_PI + M_PI) * 0.5f + 0.5f;
        spiral.vertices.push_back({cosf(i * ANVIL_PI * freq) * s, i, sinf(i * ANVIL_PI * freq) * s});
    }
    auto sprialMesh = make_mesh_from_geometry(spiral);
    sprialMesh.set_non_indexed(GL_LINE_STRIP);
    return sprialMesh;
}

inline gfx::GlMesh make_icosahedron_mesh()
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
    return make_mesh_from_geometry(icosahedron);
}

inline gfx::GlMesh make_octohedron_mesh()
{
    Geometry octohedron;
    octohedron.vertices = {
        {1.0, 0.0, 0.0}, {-1.0, 0.0, 0.0},
        {0.0, 1.0, 0.0}, {0.0, -1.0, 0.0},
        {0.0, 0.0, 1.0}, {0.0, 0.0, -1.0}
    };
    octohedron.faces = {{0, 2, 4}, {0, 4, 3}, {0, 3, 5}, {0, 5, 2}, {1, 2, 5}, {1, 5, 3}, {1, 3, 4}, {1, 4, 2}};
    octohedron.compute_normals();
    return make_mesh_from_geometry(octohedron);
}

inline gfx::GlMesh make_tetrahedron_mesh()
{
    Geometry tetrahedron;
    tetrahedron.vertices = {
        {1.0, 1.0, 1.0}, {-1.0, -1.0, 1.0},
        {-1.0, 1.0, -1.0}, {1.0, -1.0, -1.0}
    };
    tetrahedron.faces = {{2, 1, 0},{0, 3, 2},{1, 3, 0},{2, 3, 1}};
    tetrahedron.compute_normals();
    return make_mesh_from_geometry(tetrahedron);
}

inline gfx::GlMesh make_fullscreen_quad()
{
    Geometry g;
    g.vertices = { {-1.0f, -1.0f, 0.0f}, {1.0f, -1.0f, 0.0f}, {-1.0f, 1.0f, 0.0f}, {-1.0f, 1.0f, 0.0f}, {1.0f, -1.0f, 0.0f}, {1.0f, 1.0f, 0.0f} };
    g.faces = {{0, 1, 2}, {3, 4, 5}};
    return make_mesh_from_geometry(g);
}

#endif // procedural_mesh_h
