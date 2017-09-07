#pragma once

#ifndef scene_hpp
#define scene_hpp

#include "gl-api.hpp"
#include "gl-mesh.hpp"
#include "gl-camera.hpp"

#include "uniforms.hpp"
#include "assets.hpp"
#include "material.hpp"

#include "cereal/cereal.hpp"
#include "cereal/types/memory.hpp"
#include "cereal/types/vector.hpp"
#include "cereal/types/polymorphic.hpp"
#include "cereal/types/base_class.hpp"
#include "cereal/archives/json.hpp"
#include "cereal/access.hpp"

/////////////////////////////////////
//   ImGui Scene Editor Utilities  //
/////////////////////////////////////

#include "gl-imgui.hpp"
#include "imgui/imgui_internal.h"

namespace ImGui
{
    static auto vector_getter = [](void* vec, int idx, const char** out_text)
    {
        auto & vector = *static_cast<std::vector<std::string>*>(vec);
        if (idx < 0 || idx >= static_cast<int>(vector.size())) { return false; }
        *out_text = vector.at(idx).c_str();
        return true;
    };

    bool Combo(const char* label, int* currIndex, std::vector<std::string>& values)
    {
        if (values.empty()) { return false; }
        return Combo(label, currIndex, vector_getter, static_cast<void*>(&values), values.size());
    }

    bool ListBox(const char* label, int* currIndex, std::vector<std::string>& values)
    {
        if (values.empty()) { return false; }
        return ListBox(label, currIndex, vector_getter, static_cast<void*>(&values), values.size());
    }

    enum SplitType : uint32_t
    {
        Left,
        Right,
        Top,
        Bottom
    };

    typedef std::pair<Bounds2D, Bounds2D> SplitRegion;

    SplitRegion Split(const Bounds2D & r, int * v, SplitType t)
    {
        ImGuiWindow * window = ImGui::GetCurrentWindowRead();
        const ImGuiID id = window->GetID(v);
        const auto & io = ImGui::GetIO();

        float2 cursor = float2(io.MousePos);

        if (GImGui->ActiveId == id)
        {

            // Get the current mouse position relative to the desired axis
            if (io.MouseDown[0])
            {
                uint32_t position = 0;

                switch (t)
                {
                case Left:   position = cursor.x - r.min().x; break;
                case Right:  position = r.max().x - cursor.x; break;
                case Top:    position = cursor.y - r.min().y; break;
                case Bottom: position = r.max().y - cursor.y; break;
                }

                *v = position;
            }
            else ImGui::SetActiveID(0, nullptr);
        }

        SplitRegion result = { r,r };

        switch (t)
        {
        case Left:   result.first._min.x = (result.second._max.x = r.min().x + *v) + 8; break;
        case Right:  result.first._max.x = (result.second._min.x = r.max().x - *v) - 8; break;
        case Top:    result.first._min.y = (result.second._max.y = r.min().y + *v) + 8; break;
        case Bottom: result.first._max.y = (result.second._min.y = r.max().y - *v) - 8; break;
        }

        if (r.contains(cursor) && !result.first.contains(cursor) && !result.second.contains(cursor))
        {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Move);
            if (io.MouseClicked[0])
            {
                ImGui::SetActiveID(id, window);
            }
        }
        return result;
    };

}

template<class T> const T * query_metadata(const T & meta) { return &meta; }
template<class T> const T * query_metadata() { return nullptr; }
template<class T> struct range_metadata { T min, max; };
struct editor_hidden {  };

inline bool Edit(const char * label, std::string & s)
{
    char buffer[2048];
    assert(s.size() + 1 < sizeof(buffer));
    std::memcpy(buffer, s.data(), std::min(s.size() + 1, sizeof(buffer)));
    if (ImGui::InputText(label, buffer, sizeof(buffer)))
    {
        s = buffer;
        return true;
    }
    else return false;
}

inline bool Edit(const char * label, bool & v) { return ImGui::Checkbox(label, &v); }
inline bool Edit(const char * label, float & v) { return ImGui::InputFloat(label, &v); }
inline bool Edit(const char * label, int & v) { return ImGui::InputInt(label, &v); }
inline bool Edit(const char * label, int2 & v) { return ImGui::InputInt2(label, &v.x); }
inline bool Edit(const char * label, int3 & v) { return ImGui::InputInt3(label, &v.x); }
inline bool Edit(const char * label, int4 & v) { return ImGui::InputInt4(label, &v.x); }
inline bool Edit(const char * label, float2 & v) { return ImGui::InputFloat2(label, &v.x); }
inline bool Edit(const char * label, float3 & v) { return ImGui::InputFloat3(label, &v.x); }
inline bool Edit(const char * label, float4 & v) { return ImGui::InputFloat4(label, &v.x); }

// Slider for range_metadata<int>
template<class... A> bool Edit(const char * label, int & f, const A & ... metadata)
{
    auto * rangeData = query_metadata<range_metadata<int>>(metadata...);
    if (rangeData) return ImGui::SliderInt(label, &f, rangeData->min, rangeData->max);
    else return ImGui::InputInt(label, &f);
}

// Slider for range_metadata<float>
template<class... A> bool Edit(const char * label, float & f, const A & ... metadata)
{
    auto * rangeData = query_metadata<range_metadata<float>>(metadata...);
    if (rangeData) return ImGui::SliderFloat(label, &f, rangeData->min, rangeData->max, "%.5f");
    else return ImGui::InputFloat(label, &f);
}

// Slider for range_metadata<float2>
template<class... A> bool Edit(const char * label, float2 & f, const A & ... metadata)
{
    auto * floatRange = query_metadata<range_metadata<float>>(metadata...);
    if (floatRange) return ImGui::SliderFloat2(label, &f[0], floatRange->min, floatRange->max, "%.5f");
    else return ImGui::SliderFloat2(label, &f[0], 0.0f, 1.0f);
}

template<class T, class ... A> bool Edit(const char * label, AssetHandle<T> & h, const A & ... metadata)
{
    auto * hidden = query_metadata<editor_hidden>(metadata...);
    if (hidden) return false;

    int index;
    std::vector<const char *> items;

    for (auto & handle : AssetHandle<T>::list())
    {
        if (handle.name == h.name) index = static_cast<int>(items.size());
        items.push_back(handle.name.c_str());
    }

    if (ImGui::Combo(label, &index, items.data(), static_cast<int>(items.size())))
    {
        h = items[index];
        return true;
    }
    else return false;
}

template<class T> std::enable_if_t<std::is_class<T>::value, bool> Edit(const char * label, T & object)
{
    bool r = false;
    visit_fields(object, [&r](const char * name, auto & field, auto... metadata)
    {
        r |= Edit(name, field, metadata...);
    });
    return r;
}

template<class T> bool InspectGameObjectPolymorphic(const char * label, T * ptr)
{
    bool r = false;
    visit_subclasses(ptr, [&r, label](const char * name, auto * p)
    {
        if (p)
        {
            if (label) r = Edit((std::string(label) + " - " + name).c_str(), *p);
            else r = Edit(name, *p);
        }
    });
    return r;
}

///////////////////////
//   Scene Objects   //
///////////////////////

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
        float3 normal = { 0, 0, 0 };
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

    struct Renderable : public GameObject
    {
        AssetHandle<std::shared_ptr<Material>> mat;

        bool receive_shadow{ true };
        bool cast_shadow{ true };

        Material * get_material() { return mat.get().get(); }
        void set_material(AssetHandle<std::shared_ptr<Material>> handle) { mat = handle; }

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
            return{ false, -FLT_MAX,{ 0,0,0 } };
        }
    };

    struct StaticMesh final : public Renderable
    {
        Pose pose;
        float3 scale{ 1, 1, 1 };
        Bounds3D bounds;

        GlMeshHandle mesh;
        GeometryHandle geom;

        StaticMesh() {}

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


/////////////////////////////////////////
//   Engine Relationship Declarations  //
/////////////////////////////////////////

template<class F> void visit_fields(GlTextureHandle & m, F f) { f("id", m.name); }
template<class F> void visit_fields(GlShaderHandle & m, F f) { f("id", m.name); }
template<class F> void visit_fields(GlMeshHandle & m, F f) { f("id", m.name); }
template<class F> void visit_fields(GeometryHandle & m, F f) { f("id", m.name); }

template<class F> void visit_fields(Pose & o, F f)
{
    f("position", o.position);
    f("orientation", o.orientation);
}

// Standard Game Objects
template<class F> void visit_subclasses(GameObject * p, F f)
{
    f("StaticMesh", dynamic_cast<StaticMesh *>(p));
    // ...
}

template<class F> void visit_fields(GameObject & o, F f)
{
    f("id", o.id);
}

template<class F> void visit_fields(StaticMesh & o, F f)
{
    f("pose", o.pose);
    f("scale", o.scale);
    f("material", o.mat);
}

// Materials
template<class F> void visit_subclasses(Material * p, F f)
{
    f("MetallicRoughnessMaterial", dynamic_cast<MetallicRoughnessMaterial *>(p));
}

template<class F> void visit_fields(MetallicRoughnessMaterial & o, F f)
{
    f("base_albedo", o.baseAlbedo);
    f("opacity", o.opacity, range_metadata<float>{ 0.f, 1.f });
    f("roughness_factor", o.roughnessFactor, range_metadata<float>{ 0.04f, 1.f });
    f("metallic_factor", o.metallicFactor, range_metadata<float>{ 0.f, 1.f });
    f("base_emissive", o.baseEmissive);
    f("emissive_strength", o.emissiveStrength, range_metadata<float>{ 0.f, 1.f });
    f("specularLevel", o.specularLevel, range_metadata<float>{ 0.f, 2.f });
    f("occulusion_strength", o.occlusionStrength, range_metadata<float>{ 0.f, 1.f });
    f("ambient_strength", o.ambientStrength, range_metadata<float>{ 0.f, 1.f });
    f("shadow_opacity", o.shadowOpacity, range_metadata<float>{ 0.f, 1.f });
    f("texcoord_scale", o.texcoordScale, range_metadata<float>{ -32, 32 });

    f("albedo_handle", o.albedo);
    f("normal_handle", o.normal);
    f("metallic_handle", o.metallic);
    f("roughness_handle", o.roughness);
    f("emissive_handle", o.emissive);
    f("height_handle", o.height);
    f("occlusion_handle", o.occlusion);
    f("radiance_cubemap_handle", o.radianceCubemap);
    f("irradiance_cubemap_handle", o.irradianceCubemap);

    f("program_handle", o.program, editor_hidden{});
}

//////////////////////////////////////
//  Anvil Base Type Serialization   //
//////////////////////////////////////

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

////////////////////////////////////
//   Asset Handle Serialization   //
////////////////////////////////////

namespace cereal
{
    template<class Archive> void serialize(Archive & archive, GlTextureHandle & m) { archive(cereal::make_nvp("id", m.name)); }
    template<class Archive> void serialize(Archive & archive, GlShaderHandle & m) { archive(cereal::make_nvp("id", m.name)); }
    template<class Archive> void serialize(Archive & archive, GlMeshHandle & m) { archive(cereal::make_nvp("id", m.name)); }
    template<class Archive> void serialize(Archive & archive, GeometryHandle & m) { archive(cereal::make_nvp("id", m.name)); }
}

////////////////////////////////////////////////////
//   Engine Relationship Declarations For Cereal  //
////////////////////////////////////////////////////

CEREAL_REGISTER_TYPE_WITH_NAME(GameObject,                      "GameObjectBase");
CEREAL_REGISTER_TYPE_WITH_NAME(Renderable,                      "Renderable");
CEREAL_REGISTER_TYPE_WITH_NAME(StaticMesh,                      "StaticMesh");
CEREAL_REGISTER_TYPE_WITH_NAME(DirectionalLight,                "DirectionalLight");
CEREAL_REGISTER_TYPE_WITH_NAME(PointLight,                      "PointLight");
CEREAL_REGISTER_TYPE_WITH_NAME(Material,                        "MaterialBase");
CEREAL_REGISTER_TYPE_WITH_NAME(MetallicRoughnessMaterial,       "MetallicRoughnessMaterial");

CEREAL_REGISTER_POLYMORPHIC_RELATION(Renderable,                GameObject)
CEREAL_REGISTER_POLYMORPHIC_RELATION(StaticMesh,                Renderable)
CEREAL_REGISTER_POLYMORPHIC_RELATION(DirectionalLight,          Renderable)
CEREAL_REGISTER_POLYMORPHIC_RELATION(PointLight,                Renderable)
CEREAL_REGISTER_POLYMORPHIC_RELATION(MetallicRoughnessMaterial, Material);

///////////////////////////////////
//   Game Object Serialization   //
///////////////////////////////////

namespace cereal
{
    template<class Archive> void serialize(Archive & archive, GameObject & m)
    {
        visit_fields(m, [&archive](const char * name, auto & field, auto... metadata)
        {
            archive(cereal::make_nvp(name, field));
        });
    }

    template<class Archive> void serialize(Archive & archive, Renderable & m)
    {
        archive(cereal::make_nvp("game_object", cereal::base_class<GameObject>(&m)));
        archive(cereal::make_nvp("cast_shadow", m.cast_shadow));
        archive(cereal::make_nvp("receive_shadow", m.receive_shadow));
        //archive(cereal::make_nvp("material_handle", m.material));
    }

    template<class Archive> void serialize(Archive & archive, StaticMesh & m)
    {
        archive(cereal::make_nvp("renderable", cereal::base_class<Renderable>(&m)));
        archive(cereal::make_nvp("pose", m.pose));
        archive(cereal::make_nvp("scale", m.scale));
        archive(cereal::make_nvp("mesh_handle", m.mesh));
        archive(cereal::make_nvp("geometry_handle", m.geom));
    }

    template <typename T>
    void serialize_from_json(const std::string & pathToAsset, T & e)
    {
        const std::string ascii = read_file_text(pathToAsset);
        std::istringstream input_stream(ascii);
        cereal::JSONInputArchive input_archive(input_stream);
        input_archive(e);
    }

    template <typename T>
    std::string serialize_to_json(T e)
    {
        std::ostringstream oss;
        {
            cereal::JSONOutputArchive json(oss);
            json(e);
        }
        return oss.str();
    }
}


///////////////////////////////////////
//   Material System Serialization   //
///////////////////////////////////////

namespace cereal
{
    template<class Archive> void serialize(Archive & archive, MetallicRoughnessMaterial & m)
    {
        visit_fields(m, [&archive](const char * name, auto & field, auto... metadata)
        {
            archive(cereal::make_nvp(name, field));
        });
    }
}
#endif
