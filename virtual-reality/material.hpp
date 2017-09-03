#pragma once

#ifndef vr_material_hpp
#define vr_material_hpp

#include "gl-api.hpp"
#include "linalg_util.hpp"
#include "geometric.hpp"
#include "assets.hpp"

struct RenderPassData;

namespace avl
{

    struct Material
    {
        GlShaderHandle program;
        virtual void update_uniforms(const RenderPassData * data) {}
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
        GlTextureHandle occlusion;
        GlTextureHandle radianceCubemap;
        GlTextureHandle irradianceCubemap;

        float roughnessFactor{ 0.5f };
        float metallicFactor{ 1.f };
        float ambientIntensity{ 1.f };

        float3 emissiveColor{ float3(1, 1, 1) };
        float emissiveStrength{ 1.f };
        float overshadowConstant{ 64.f };

    public:

        MetallicRoughnessMaterial(GlShaderHandle shader);

        void update_uniforms(const RenderPassData * data) override;
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
        void set_occulusion_texture(GlTextureHandle asset) { occlusion = asset; }
        void set_radiance_cubemap(GlTextureHandle asset) { radianceCubemap = asset; }
        void set_irrradiance_cubemap(GlTextureHandle asset) { irradianceCubemap = asset; }
    };

}

#endif // end vr_material_hpp
