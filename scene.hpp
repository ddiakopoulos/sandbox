#pragma once

#ifndef scene_h
#define scene_h

#include "GL_API.hpp"
#include "geometry.hpp"
#include "camera.hpp"

namespace avl
{
    
    struct Object
    {
        Pose pose;
        float3 scale;
        Object() : scale(1, 1, 1) {}
        float4x4 get_model() const { return mul(pose.matrix(), make_scaling_matrix(scale)); }
        Box<float, 3> bounds;
    };

    struct Renderable : public Object
    {
        GlMesh mesh;
        Geometry geom;
        
        Renderable() {}
        
        Renderable(const Geometry & g) : geom(g)
        {
            rebuild_mesh();
            //mesh.set_non_indexed(GL_LINES);
            //glPointSize(8);
        }
        
        void rebuild_mesh() { bounds = geom.compute_bounds(); mesh = make_mesh_from_geometry(geom); }
        
        void draw() const { mesh.draw_elements(); };
        
        std::tuple<bool, float, float3> check_hit(const Ray & worldRay) const
        {
            auto localRay = pose.inverse() * worldRay;
            localRay.origin /= scale;
            localRay.direction /= scale;
            float outT = 0.0f;
            float3 outNormal = {0, 0, 0};
            bool hit = intersect_ray_mesh(localRay, geom, &outT, &outNormal);
            return {hit, outT, outNormal};
        }
    };

    struct LightObject : public Object
    {
        float3 color;
    };

    struct Raycast
    {
        GlCamera & cam; float2 viewport;
        Raycast(GlCamera & camera, float2 viewport) : cam(camera), viewport(viewport) {}
        Ray from(float2 cursor) { return cam.get_world_ray(cursor, viewport); };
    };
    
}

#endif