#ifndef geometry_H
#define geometry_H

#include "linear_algebra.hpp"
#include "math_util.hpp"
#include "geometric.hpp"

#include <vector>

namespace util
{
    
    struct Geometry
    {
        std::vector<math::float3> vertices;
        std::vector<math::float3> normals;
        std::vector<math::float4> colors;
        std::vector<math::float2> uv;
        std::vector<math::float3> tangents;
        std::vector<math::float3> bitangents;
        std::vector<math::uint3> faces;
        
        void compute_normals()
        {
            normals.resize(vertices.size());
            for (auto & n : normals)
                n = math::float3(0,0,0);
            
            for (const auto & tri : faces)
            {
                math::float3 n = math::cross(vertices[tri.y] - vertices[tri.x], vertices[tri.z] - vertices[tri.x]);
                normals[tri.x] += n;
                normals[tri.y] += n;
                normals[tri.z] += n;
            }
            
            for (auto & n : normals)
                n = math::normalize(n);
        }
        
    };
    
    struct Model
    {
        Geometry g;
        math::Pose pose;
        math::Box<float, 3> bounds;
    };
    
    struct Mesh : public Model
    {
        Mesh()
        {
            
        }
        
        Mesh(Mesh && r)
        {
            
        }
        
        Mesh(const Mesh & r) = delete;
        
        Mesh & operator = (Mesh && r)
        {
            return *this;
        }
        
        Mesh & operator = (const Mesh & r) = delete;
        
        ~Mesh();
        
        void draw() const;
        void update() const;
    };
    
    Mesh make_mesh(Model & m)
    {
        return Mesh();
    }
    
} // end namespace util

#endif // geometry_h
