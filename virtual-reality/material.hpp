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
        std::shared_ptr<GlShader> program;
        virtual void update_uniforms(const RenderPassData * data) {}
        virtual void use(const float4x4 & modelMatrix, const float4x4 & viewMatrix) {}
        uint32_t id() { return (uint32_t) program->handle(); }
    };

    class DebugMaterial : public Material
    {
    public:
        DebugMaterial(std::shared_ptr<GlShader> shader);
        virtual void use(const float4x4 & modelMatrix, const float4x4 & viewMatrix) override;
    };

    class WireframeMaterial : public Material
    {
    public:
        WireframeMaterial(std::shared_ptr<GlShader> shader);
        virtual void use(const float4x4 & modelMatrix, const float4x4 & viewMatrix) override;
    };

    class TexturedMaterial : public Material
    {
        GlTexture2D diffuseTexture;
    public:
        TexturedMaterial(std::shared_ptr<GlShader> shader);
        virtual void update_uniforms(const RenderPassData * data) override;
        virtual void use(const float4x4 & modelMatrix, const float4x4 & viewMatrix) override;
        void set_diffuse_texture(GlTexture2D & tex);
    };

    class MetallicRoughnessMaterial : public Material
    {
        texture_handle albedo;
        texture_handle normal;
        texture_handle metallic;
        texture_handle roughness;
        texture_handle emissive;
        texture_handle occlusion;
        texture_handle radianceCubemap;
        texture_handle irradianceCubemap;

        float roughnessFactor{ 1.f };
        float metallicFactor{ 1.f };
        float ambientIntensity{ 1.f };

        float3 emissiveColor{ float3(1, 1, 1) };
        float emissiveStrength{ 1.f };

    public:

        MetallicRoughnessMaterial(std::shared_ptr<GlShader> shader);

        virtual void update_uniforms(const RenderPassData * data) override;
        virtual void use(const float4x4 & modelMatrix, const float4x4 & viewMatrix) override;

        void set_emissive_strength(const float & strength) { emissiveStrength = strength; }
        void set_emissive_color(const float3 & color) { emissiveColor = color; }
        void set_roughness(const float & value) { roughnessFactor = value; }
        void set_metallic(const float & value) { metallicFactor = value; }
        void set_ambientIntensity(const float & value) { ambientIntensity = value; }
        void set_albedo_texture(texture_handle asset) { albedo = asset; }
        void set_normal_texture(texture_handle asset) { normal = asset; }
        void set_metallic_texture(texture_handle asset) { metallic = asset; }
        void set_roughness_texture(texture_handle asset) { roughness = asset; }
        void set_emissive_texture(texture_handle asset) { emissive = asset; }
        void set_occulusion_texture(texture_handle asset) { occlusion = asset; }
        void set_radiance_cubemap(texture_handle asset) { radianceCubemap = asset; }
        void set_irrradiance_cubemap(texture_handle asset) { irradianceCubemap = asset; }
    };

}

#endif // end vr_material_hpp
