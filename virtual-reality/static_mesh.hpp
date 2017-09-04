#pragma once

#ifndef vr_static_mesh_hpp
#define vr_static_mesh_hpp

#include "material.hpp"

//#include "bullet_engine.hpp"
//#include "bullet_object.hpp"

//BulletObjectVR * physicsComponent{ nullptr };

/*
void set_physics_component(BulletObjectVR * obj)
{
    physicsComponent = obj;
}

BulletObjectVR * get_physics_component() const
{
    return physicsComponent;
}
*/

class StaticMesh final : public Renderable
{
    Pose pose;
    float3 scale{ 1, 1, 1 };

    GlMeshHandle mesh;
    GeometryHandle geom;
    Bounds3D bounds;

public:

    StaticMesh(GlMeshHandle m, GeometryHandle g) : mesh(m), geom(g) { }

    Pose get_pose() const override { return pose; }
    void set_pose(const Pose & p) override { pose = p; }
    Bounds3D get_bounds() const override { return bounds; }
    float3 get_scale() const override { return scale; }
    void set_scale(const float3 & s) override { scale = s; }

    void draw() const override 
    {
        mesh.get().draw_elements(); 
    }

    void update(const float & dt) override { }

    Bounds3D get_world_bounds() const override
    {
        const Bounds3D local = get_bounds();
        const float3 scale = get_scale();
        return{ pose.transform_coord(local.min()) * scale, pose.transform_coord(local.max()) * scale };
    }

    RaycastResult raycast(const Ray & worldRay) const override
    {
        auto localRay = pose.inverse() * worldRay;
        localRay.origin /= scale;
        localRay.direction /= scale;
        float outT = 0.0f;
        float3 outNormal = { 0, 0, 0 };
        bool hit = intersect_ray_mesh(localRay, geom.get(), &outT, &outNormal);
        return{ hit, outT, outNormal };
    }

    void set_mesh_render_mode(GLenum renderMode)
    {
        if (renderMode != GL_TRIANGLE_STRIP) mesh.get().set_non_indexed(renderMode);
    }
};

#endif // end vr_static_mesh_hpp
