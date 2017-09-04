#pragma once

#ifndef vr_material_hpp
#define vr_material_hpp

#include "gl-api.hpp"
#include "linalg_util.hpp"
#include "geometric.hpp"
#include "assets.hpp"

namespace avl
{

    struct Material
    {
        GlShaderHandle program;
        virtual void update_uniforms() {}
        virtual void use() {}
        uint32_t id() const { return program.get().handle(); }
        virtual ~Material() {}
    };

    class DebugMaterial final : public Material
    {
    public:
        DebugMaterial(GlShaderHandle shader);
        void use() override;
    };

    class MetallicRoughnessMaterial final : public Material
    {
        GlTextureHandle albedo;
        GlTextureHandle normal;
        GlTextureHandle metallic;
        GlTextureHandle roughness;
        GlTextureHandle emissive;
        GlTextureHandle height;
        GlTextureHandle occlusion;
        GlTextureHandle radianceCubemap;
        GlTextureHandle irradianceCubemap;

        int bindpoint = 0;

        float roughnessFactor{ 0.5f };
        float metallicFactor{ 1.f };
        float ambientIntensity{ 1.f };

        float3 emissiveColor{ float3(1, 1, 1) };
        float emissiveStrength{ 1.f };
        float overshadowConstant{ 64.f };

    public:

        MetallicRoughnessMaterial() {}

        MetallicRoughnessMaterial(GlShaderHandle shader);

        void update_cascaded_shadow_array_handle(GLuint handle);
        void update_uniforms() override;
        void use() override;

        //void set_emissive_strength(const float strength) { emissiveStrength = strength; }
        //void set_emissive_color(const float3 & color) { emissiveColor = color; }
        void set_roughness(const float value) { roughnessFactor = value; }
        void set_metallic(const float value) { metallicFactor = value; }
        //void set_ambientIntensity(const float value) { ambientIntensity = value; }
        //void setOvershadowConstant(const float value) { overshadowConstant = value; }

        void set_albedo_texture(GlTextureHandle asset) { albedo = asset; }
        void set_normal_texture(GlTextureHandle asset) { normal = asset; }
        void set_metallic_texture(GlTextureHandle asset) { metallic = asset; }
        void set_roughness_texture(GlTextureHandle asset) { roughness = asset; }
        void set_emissive_texture(GlTextureHandle asset) { emissive = asset; }
        void set_height_texture(GlTextureHandle asset) { height = asset; }
        void set_occulusion_texture(GlTextureHandle asset) { occlusion = asset; }
        void set_radiance_cubemap(GlTextureHandle asset) { radianceCubemap = asset; }
        void set_irrradiance_cubemap(GlTextureHandle asset) { irradianceCubemap = asset; }
    };

}

typedef AssetHandle<avl::Material> MaterialHandle;

/*
#include "cereal/cereal.hpp"
#include "cereal/types/memory.hpp"
#include "cereal/types/vector.hpp"
#include "cereal/types/polymorphic.hpp"
#include "cereal/types/base_class.hpp"
#include "cereal/archives/json.hpp"

CEREAL_REGISTER_TYPE_WITH_NAME(avl::Material, "MaterialBase");
CEREAL_REGISTER_TYPE_WITH_NAME(avl::DebugMaterial, "DebugMaterial");
CEREAL_REGISTER_TYPE_WITH_NAME(avl::MetallicRoughnessMaterial, "MetallicRoughnessMaterial");
CEREAL_REGISTER_POLYMORPHIC_RELATION(avl::DebugMaterial, avl::Material)
CEREAL_REGISTER_POLYMORPHIC_RELATION(avl::MetallicRoughnessMaterial, avl::Material)

namespace cereal
{
    template<class Archive> void serialize(Archive & archive, Material & m)
    {
        archive(cereal::make_nvp("program_handle", m.program.asset_id()));
    }

    template<class Archive> void serialize(Archive & archive, MetallicRoughnessMaterial & m)
    {
        archive(cereal::make_nvp("material", cereal::base_class<Material>(&m)));
        archive(cereal::make_nvp("albedo_handle", m.albedo.asset_id()));
        archive(cereal::make_nvp("normal_handle", m.normal.asset_id()));
        archive(cereal::make_nvp("metallic_handle", m.metallic.asset_id()));
        archive(cereal::make_nvp("roughness_handle", m.roughness.asset_id()));
        archive(cereal::make_nvp("emissive_handle", m.emissive.asset_id()));
        archive(cereal::make_nvp("height_handle", m.height.asset_id()));
        archive(cereal::make_nvp("occlusion_handle", m.occlusion.asset_id()));
        archive(cereal::make_nvp("radiance_cubemap_handle", m.radianceCubemap.asset_id()));
        archive(cereal::make_nvp("irradiance_cubemap_handle", m.irradianceCubemap.asset_id()));
        // other properties here
    }
}
*/

#endif // end vr_material_hpp
