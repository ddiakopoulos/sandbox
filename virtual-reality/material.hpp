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

    class MetallicRoughnessMaterial final : public Material
    {
        int bindpoint = 0;

    public:

        MetallicRoughnessMaterial() {}
        void update_cascaded_shadow_array_handle(GLuint handle);
        void update_uniforms() override;
        void use() override;

        float3 baseAlbedo{ float3(1, 1, 1) };
        float opacity{ 1.f };

        float roughnessFactor{ 0.04f };
        float metallicFactor{ 1.f };

        float3 baseEmissive{ float3(0, 0, 0) };
        float emissiveStrength{ 1.f };

        float specularLevel{ 0.04f };
        float occlusionStrength{ 1.f };
        float ambientStrength{ 1.f };
        float shadowOpacity{ 0.9f };

        float2 texcoordScale{ 4.f, 4.f };

        GlTextureHandle albedo;
        GlTextureHandle normal;
        GlTextureHandle metallic;
        GlTextureHandle roughness;
        GlTextureHandle emissive;
        GlTextureHandle height;
        GlTextureHandle occlusion;
        GlTextureHandle radianceCubemap;
        GlTextureHandle irradianceCubemap;
    };

}

class RuntimeMaterialInstance
{
    void associate()
    {
        // Iterate all material types? 
        for (auto & m : AssetHandle<MetallicRoughnessMaterial>::list())
        {
            if (m.name.size() > 0 && m.name == name)
            {
                std::cout << "Assigning material: " << name << std::endl;
                mat = &m.get();
            }
        }
    }

public:
    std::string name;
    Material * mat{ nullptr };
    RuntimeMaterialInstance() { associate(); }
    RuntimeMaterialInstance(const std::string & name) : name(name) { associate(); }
    Material * get() { return mat; }
};

typedef AssetHandle<RuntimeMaterialInstance> MaterialHandle;

#endif // end vr_material_hpp
