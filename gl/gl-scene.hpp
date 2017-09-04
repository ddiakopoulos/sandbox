#pragma once

#ifndef scene_hpp
#define scene_hpp

#include "gl-api.hpp"
#include "gl-mesh.hpp"
#include "gl-camera.hpp"
#include "../virtual-reality/uniforms.hpp"
#include "../virtual-reality/assets.hpp"

namespace avl
{

    struct ViewportRaycast
    {
        GlCamera & cam; float2 viewport;
        ViewportRaycast(GlCamera & camera, float2 viewport) : cam(camera), viewport(viewport) {}
        Ray from(float2 cursor) { return cam.get_world_ray(cursor, viewport); };
    };

    struct RaycastResult
    {
        bool hit = false;
        float distance = std::numeric_limits<float>::max();
        float3 normal = {0, 0, 0};
        RaycastResult(bool h, float t, float3 n) : hit(h), distance(t), normal(n) {}
    };

    struct GameObject
    {
        virtual ~GameObject() {}
        virtual void update(const float & dt) {}
        virtual void draw() const {};
        virtual Bounds3D get_world_bounds() const = 0;
        virtual Bounds3D get_bounds() const = 0;
        virtual float3 get_scale() const = 0;
        virtual void set_scale(const float3 & s) = 0;
        virtual Pose get_pose() const = 0;
        virtual void set_pose(const Pose & p) = 0;
        virtual RaycastResult raycast(const Ray & worldRay) const = 0;
    };

    class Material;
    struct Renderable : public GameObject
    {
        Material * material{ nullptr };
        bool receive_shadow{ true };
        bool cast_shadow{ true };

        Material * get_material() const { return material; }
        void set_material(Material * const m) { material = m; }

        void set_receive_shadow(const bool value) { receive_shadow = value; }
        bool get_receive_shadow() const { return receive_shadow; }

        void set_cast_shadow(const bool value) { cast_shadow = value; }
        bool get_cast_shadow() const { return cast_shadow; }
    };
    
    struct PointLight final : public Renderable
    {
        uniforms::point_light data;

        PointLight()
        {
            receive_shadow = false;
            cast_shadow = false;
        }

        Pose get_pose() const override { return Pose(float4(0, 0, 0, 1), data.position); }
        void set_pose(const Pose & p) override { data.position = p.position; }
        Bounds3D get_bounds() const override { return Bounds3D(float3(-0.5f), float3(0.5f)); }
        float3 get_scale() const override { return float3(1, 1, 1); }
        void set_scale(const float3 & s) override { /* no-op */ }

        void draw() const override
        {
            GlShaderHandle("wireframe").get().bind();
            GlMeshHandle("icosphere").get().draw_elements();
        }

        Bounds3D get_world_bounds() const override
        {
            const Bounds3D local = get_bounds();
            return{ get_pose().transform_coord(local.min()), get_pose().transform_coord(local.max()) };
        }

        RaycastResult raycast(const Ray & worldRay) const override
        {
            auto localRay = get_pose().inverse() * worldRay;
            float outT = 0.0f;
            float3 outNormal = { 0, 0, 0 };
            bool hit = intersect_ray_sphere(localRay, Sphere(data.position, 1.0f), &outT, &outNormal);
            return{ hit, outT, outNormal };
        }
    };

    struct DirectionalLight final : public Renderable
    {
        uniforms::directional_light data;

        DirectionalLight()
        {
            receive_shadow = false;
            cast_shadow = false;
        }

        Pose get_pose() const override 
        { 
            auto directionQuat = make_quat_from_to({ 0, 1, 0 }, data.direction);
            return Pose(directionQuat);
        }
        void set_pose(const Pose & p) override 
        { 
            data.direction = qydir(p.orientation);
        }
        Bounds3D get_bounds() const override { return Bounds3D(float3(-0.5f), float3(0.5f)); }
        float3 get_scale() const override { return float3(1, 1, 1); }
        void set_scale(const float3 & s) override { /* no-op */ }

        void draw() const override
        {
            GlShaderHandle("wireframe").get().bind();
            GlMeshHandle("icosphere").get().draw_elements();
        }

        Bounds3D get_world_bounds() const override
        {
            const Bounds3D local = get_bounds();
            return{ get_pose().transform_coord(local.min()), get_pose().transform_coord(local.max()) };
        }

        RaycastResult raycast(const Ray & worldRay) const override
        {
            return{ false, -FLT_MAX, {0,0,0} };
        }
    };

    struct DebugRenderable
    {
        virtual void draw(const float4x4 & viewProj) = 0;
    };

    class SimpleStaticMesh
    {
        Pose pose;
        float3 scale{ 1, 1, 1 };
        GlMesh mesh;
        Geometry geom;
        Bounds3D bounds;

    public:

        SimpleStaticMesh() {}

        Pose get_pose() const { return pose; }
        void set_pose(const Pose & p) { pose = p; }
        float3 get_scale() const { return scale; }
        void set_scale(const float3 & s) { scale = s; }
        Bounds3D get_bounds() const { return bounds; }
        Geometry & get_geometry() { return geom; }
        void draw() const { mesh.draw_elements(); }
        void update(const float & dt) { }

        Bounds3D get_world_bounds() const
        {
            const Bounds3D local = get_bounds();
            const float3 scale = get_scale();
            return{ pose.transform_coord(local.min()) * scale, pose.transform_coord(local.max()) * scale };
        }

        RaycastResult raycast(const Ray & worldRay) const
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
    };

} // end namespace avl

#endif
