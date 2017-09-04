#pragma once

#ifndef scene_hpp
#define scene_hpp

#include "gl-api.hpp"
#include "gl-mesh.hpp"
#include "gl-camera.hpp"

#include "../virtual-reality/uniforms.hpp"
#include "../virtual-reality/assets.hpp"
#include "../virtual-reality/material.hpp"

#include "cereal/cereal.hpp"
#include "cereal/types/memory.hpp"
#include "cereal/types/vector.hpp"
#include "cereal/types/polymorphic.hpp"
#include "cereal/types/base_class.hpp"
#include "cereal/archives/json.hpp"
#include "cereal/access.hpp"

namespace avl
{
    struct DebugRenderable
    {
        virtual void draw(const float4x4 & viewProj) = 0;
    };

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
        std::string id;
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
        Material * mat{ nullptr };

        bool receive_shadow{ true };
        bool cast_shadow{ true };

        Material * get_material() const { return mat; }
        void set_material(Material * m) 
        {
            std::cout << "Setting Material: " << m << std::endl;
            mat = m; 
        }

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
            //GlShaderHandle("wireframe").get().bind();
            //GlMeshHandle("icosphere").get().draw_elements();
            //GlShaderHandle("wireframe").get().unbind();
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
            //GlShaderHandle("wireframe").get().bind();
            //GlMeshHandle("icosphere").get().draw_elements();
            //GlShaderHandle("wireframe").get().unbind();
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

    class StaticMesh final : public Renderable
    {
        Pose pose;
        float3 scale{ 1, 1, 1 };
        Bounds3D bounds;

    public:

        GlMeshHandle mesh;
        GeometryHandle geom;

        StaticMesh(GlMeshHandle m, GeometryHandle g) : mesh(m), geom(g) { }

        StaticMesh(StaticMesh && r)
        {
            *this = std::move(r);
        }

        StaticMesh & operator = (StaticMesh && r)
        {
            std::swap(pose, r.pose);
            std::swap(scale, r.scale);
            std::swap(bounds, r.bounds);
            std::swap(mesh, r.mesh);
            std::swap(geom, r.geom);
            std::swap(mat, r.mat);
            return *this;
        }

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


} // end namespace avl

namespace cereal
{
    template<class Archive> void serialize(Archive & archive, float2 & m) { archive(cereal::make_nvp("x", m.x), cereal::make_nvp("y", m.y)); }
    template<class Archive> void serialize(Archive & archive, float3 & m) { archive(cereal::make_nvp("x", m.x), cereal::make_nvp("y", m.y), cereal::make_nvp("z", m.z)); }
    template<class Archive> void serialize(Archive & archive, float4 & m) { archive(cereal::make_nvp("x", m.x), cereal::make_nvp("y", m.y), cereal::make_nvp("z", m.z), cereal::make_nvp("w", m.w)); }

    template<class Archive> void serialize(Archive & archive, float2x2 & m) { archive(cereal::make_size_tag(2), m[0], m[1]); }
    template<class Archive> void serialize(Archive & archive, float3x3 & m) { archive(cereal::make_size_tag(3), m[0], m[1], m[2]); }
    template<class Archive> void serialize(Archive & archive, float4x4 & m) { archive(cereal::make_size_tag(4), m[0], m[1], m[2], m[3]); }
    template<class Archive> void serialize(Archive & archive, Frustum & m) { archive(cereal::make_size_tag(6)); for (auto const & p : m.planes) archive(p); }

    template<class Archive> void serialize(Archive & archive, Pose & m) { archive(cereal::make_nvp("position", m.position), cereal::make_nvp("orientation", m.orientation)); }
    template<class Archive> void serialize(Archive & archive, Bounds2D & m) { archive(cereal::make_nvp("min", m._min), cereal::make_nvp("max", m._max)); }
    template<class Archive> void serialize(Archive & archive, Bounds3D & m) { archive(cereal::make_nvp("min", m._min), cereal::make_nvp("max", m._max)); }
    template<class Archive> void serialize(Archive & archive, Ray & m) { archive(cereal::make_nvp("origin", m.origin), cereal::make_nvp("direction", m.direction)); }
    template<class Archive> void serialize(Archive & archive, Plane & m) { archive(cereal::make_nvp("equation", m.equation)); }
    template<class Archive> void serialize(Archive & archive, Line & m) { archive(cereal::make_nvp("origin", m.origin), cereal::make_nvp("direction", m.direction)); }
    template<class Archive> void serialize(Archive & archive, Segment & m) { archive(cereal::make_nvp("a", m.a), cereal::make_nvp("b", m.b)); }
    template<class Archive> void serialize(Archive & archive, Sphere & m) { archive(cereal::make_nvp("center", m.center), cereal::make_nvp("radius", m.radius)); }
}

CEREAL_REGISTER_TYPE_WITH_NAME(GameObject, "GameObject");
CEREAL_REGISTER_TYPE_WITH_NAME(Renderable, "Renderable");
CEREAL_REGISTER_TYPE_WITH_NAME(StaticMesh, "StaticMesh");
CEREAL_REGISTER_TYPE_WITH_NAME(DirectionalLight, "DirectionalLight");
CEREAL_REGISTER_TYPE_WITH_NAME(PointLight, "PointLight");

CEREAL_REGISTER_POLYMORPHIC_RELATION(Renderable, GameObject)

CEREAL_REGISTER_POLYMORPHIC_RELATION(StaticMesh, Renderable)
CEREAL_REGISTER_POLYMORPHIC_RELATION(DirectionalLight, Renderable)
CEREAL_REGISTER_POLYMORPHIC_RELATION(PointLight, Renderable)

namespace cereal
{
    template<class Archive> void serialize(Archive & archive, GameObject & m)
    {
        archive(cereal::make_nvp("id", m.id));
    }

    template<class Archive> void serialize(Archive & archive, Renderable & m)
    {
        archive(cereal::make_nvp("game_object", cereal::base_class<GameObject>(&m)));
        archive(cereal::make_nvp("cast_shadow", m.cast_shadow));
        archive(cereal::make_nvp("receive_shadow", m.receive_shadow));
        //archive(cereal::make_nvp("material", m.material.asset_id()));
    }

    template <> 
    struct LoadAndConstruct<StaticMesh>
    {
        template <class Archive>
        static void load_and_construct(Archive & archive, cereal::construct<StaticMesh> & construct)
        {
            std::string mesh_handle;
            std::string geometry_handle;
            archive(cereal::make_nvp("mesh_handle", mesh_handle), cereal::make_nvp("geometry_handle", geometry_handle));
            construct(mesh_handle, geometry_handle);
        }
    };

    template<class Archive> void serialize(Archive & archive, StaticMesh & m)
    {
        archive(cereal::make_nvp("renderable", cereal::base_class<Renderable>(&m)));
        archive(cereal::make_nvp("pose", m.get_pose()));
        archive(cereal::make_nvp("scale", m.get_scale()));
        archive(cereal::make_nvp("bounds", m.get_bounds()));
        archive(cereal::make_nvp("mesh_handle", m.mesh.asset_id()));
        archive(cereal::make_nvp("geometry_handle", m.geom.asset_id()));
    }

    template <typename T>
    std::string ToJson(T e)
    {
        std::ostringstream oss;
        {
            cereal::JSONOutputArchive json(oss);
            json(e);
        }
        return oss.str();
    }
}


#endif
