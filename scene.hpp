#pragma once

#ifndef scene_h
#define scene_h

#include "GlShared.hpp"
#include "geometry.hpp"

struct Object
{
    math::Pose pose;
    math::float3 scale;
    Object() : scale(1, 1, 1) {}
    math::float4x4 get_model() const { return mul(pose.matrix(), make_scaling_matrix(scale)); }
    math::Box<float, 3> bounds;
};

struct Renderable : public Object
{
    gfx::GlMesh mesh;
    util::Geometry geom;
    
    Renderable() {}
    
    Renderable(const util::Geometry & g) : geom(g)
    {
        mesh = make_mesh_from_geometry(g);
        bounds = g.compute_bounds();
        //mesh.set_non_indexed(GL_TRIANGLE_STRIP);
        //glPointSize(8);
    }
    
    void draw() const { mesh.draw_elements(); };
    
    bool check_hit(const gfx::Ray & worldRay, float * out = nullptr) const
    {
        auto localRay = pose.inverse() * worldRay;
        localRay.origin /= scale;
        localRay.direction /= scale;
        float outT = 0.0f;
        bool hit = intersect_ray_mesh(localRay, geom, &outT);
        if (out) *out = outT;
        return hit;
    }
};

struct LightObject : public Object
{
    math::float3 color;
};

struct Raycast
{
    gfx::GlCamera & cam; math::float2 viewport;
    Raycast(gfx::GlCamera & camera, math::float2 viewport) : cam(camera), viewport(viewport) {}
    gfx::Ray from(math::float2 cursor) { return cam.get_world_ray(cursor, viewport); };
};

#endif