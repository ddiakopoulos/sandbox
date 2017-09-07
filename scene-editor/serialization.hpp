#pragma once

#ifndef serialization_hpp
#define serialization_hpp

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

// There are two distinct but related bits of functionality here. The frist is the `visit_fields` and `visit_subclasses` subclass paradigm,
// which reflects class properties and defines polymorphic relationships, respectively. ImGui uses these definitions to build object
// interfaces in the scene editor as defined in `gui.hpp`. Furthermore, metadata attributes can be provided as a third argument in `visit_fields`, 
// whereby ImGui can be made aware of additional relevant properties (like hiding the field in the inspector or defining min/max ranges). The 
// second bit of functionality makes use of the C++11 Cereal library. Unfortunately, Cereal requires a further definition of polymorphic
// relationships in its own format, however JSON serialization falls through to an existing `visit_field` or `visit_subclass` template.

/////////////////////////////////////////
//   Engine Relationship Declarations  //
/////////////////////////////////////////

template<class F> void visit_fields(GlTextureHandle & m, F f) { f("id", m.name); }
template<class F> void visit_fields(GlShaderHandle & m, F f) { f("id", m.name); }
template<class F> void visit_fields(GlMeshHandle & m, F f) { f("id", m.name); }
template<class F> void visit_fields(GeometryHandle & m, F f) { f("id", m.name); }

template<class F> void visit_fields(Pose & o, F f) { f("position", o.position); f("orientation", o.orientation); }

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

////////////////////////////////
//  Base Type Serialization   //
////////////////////////////////

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
    template <typename T>
    void deserialize_from_json(const std::string & pathToAsset, T & e)
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

#endif // end serialization_hpp
