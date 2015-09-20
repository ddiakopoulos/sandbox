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
        std::vector<math::uint3>  faces;
    };
    
    struct Model
    {
        Geometry g;
        math::Pose pose;
        math::Box<float, 3> bounds;
    };
    
    struct RenderableModel : public Model
    {
        void draw();
        void update();
    };
    
    void compute_normals(Geometry & g)
    {
        
    }
    
    void compute_tangents(Geometry & g)
    {
        
    }
    
} // end namespace util

#endif // geometry_H
