#pragma once

#ifndef vr_static_mesh_hpp
#define vr_static_mesh_hpp

#include "material.hpp"
//#include "bullet_engine.hpp"
//#include "bullet_object.hpp"

class StaticMesh : public Renderable
{
    Pose pose;
    float3 scale{ 1, 1, 1 };

    GlMesh mesh;
    Geometry geom;
    Bounds3D bounds;

    Material * material;

    //BulletObjectVR * physicsComponent{ nullptr };

public:

    StaticMesh() {}

    virtual Pose get_pose() const override { return pose; }
    virtual void set_pose(const Pose & p) override { pose = p; }
    virtual Bounds3D get_bounds() const override { return bounds; }
    virtual float3 get_scale() const override { return scale; }
    virtual void set_scale(const float3 & s) { scale = s; }
    virtual void draw() const override { mesh.draw_elements(); }
    virtual void update(const float & dt) override { }
    virtual Material * get_material() const override { return material; }
    virtual void set_material(Material * const m) override { material = m; }

    virtual Bounds3D get_world_bounds() const
    {
        const Bounds3D local = get_bounds();
        const float3 scale = get_scale();
        return{ pose.transform_coord(local.min()) * scale, pose.transform_coord(local.max()) * scale };
    }

    virtual RaycastResult raycast(const Ray & worldRay) const override
    {
        auto localRay = pose.inverse() * worldRay;
        localRay.origin /= scale;
        localRay.direction /= scale;
        float outT = 0.0f;
        float3 outNormal = { 0, 0, 0 };
        bool hit = intersect_ray_mesh(localRay, geom, &outT, &outNormal);
        return{ hit, outT, outNormal };
    }

    void set_static_mesh(const Geometry & g, const float scale = 1.f, const GLenum usage = GL_STATIC_DRAW)
    {
        geom = g;
        if (scale != 1.f) rescale_geometry(geom, scale);
        bounds = geom.compute_bounds();
        mesh = make_mesh_from_geometry(geom, usage);
    }

    void set_mesh_render_mode(GLenum renderMode)
    {
        if (renderMode != GL_TRIANGLE_STRIP) mesh.set_non_indexed(renderMode);
    }

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
};

#endif // end vr_static_mesh_hpp
