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
    };

    struct MetallicRoughnessMaterial final : public Material
    {
        int bindpoint = 0;

    public:

        float roughnessFactor{ 0.5f };
        float metallicFactor{ 1.f };
        float ambientIntensity{ 1.f };

        float3 emissiveColor{ float3(0, 0, 0) };
        float emissiveStrength{ 1.f };
        float overshadowConstant{ 64.f };

        GlTextureHandle albedo;
        GlTextureHandle normal;
        GlTextureHandle metallic;
        GlTextureHandle roughness;
        GlTextureHandle emissive;
        GlTextureHandle height;
        GlTextureHandle occlusion;
        GlTextureHandle radianceCubemap;
        GlTextureHandle irradianceCubemap;

        MetallicRoughnessMaterial() {} // Why does this need a default constructor? 

        MetallicRoughnessMaterial(GlShaderHandle shader);

        void update_cascaded_shadow_array_handle(GLuint handle);
        void update_uniforms() override;
        void use() override;
    };

}

typedef AssetHandle<avl::MetallicRoughnessMaterial> MetallicRoughnessMaterialHandle;

#endif // end vr_material_hpp
