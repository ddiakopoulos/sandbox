#pragma once

#ifndef vr_material_hpp
#define vr_material_hpp

#include "gl-api.hpp"
#include "linalg_util.hpp"
#include "geometric.hpp"
#include "assets.hpp"

template<class F, class T> void visit_fields(T & t, F f)
{
    t.visit_fields(f);
}

template <unsigned int I = 0, class F, class... TS>
inline typename std::enable_if<I == sizeof...(TS), void>::type visit_fields(std::tuple<TS...> & params, F f)
{
}

template <unsigned int I = 0, class F, class... TS>
inline typename std::enable_if< I < sizeof...(TS), void>::type visit_fields(std::tuple<TS...> &params, F f)
{
    f(std::get<I + 1>(params), std::get<I>(params));
    visit_fields<I + 2, F, TS...>(params, f);
}

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

        float2 texcoordScale{ 1.f, 2.f };

        GlTextureHandle albedo;
        GlTextureHandle normal;
        GlTextureHandle metallic;
        GlTextureHandle roughness;
        GlTextureHandle emissive;
        GlTextureHandle height;
        GlTextureHandle occlusion;
        GlTextureHandle radianceCubemap;
        GlTextureHandle irradianceCubemap;

        MetallicRoughnessMaterial() {}
        void update_cascaded_shadow_array_handle(GLuint handle);
        void update_uniforms() override;
        void use() override;
    };

}

template<class F> void visit_fields(MetallicRoughnessMaterial & o, F f)
{
    f("1", o.baseAlbedo);
    f("2", o.height);
    f("3", o.shadowOpacity);
    f("4", o.texcoordScale);
};

struct field_encoder 
{ 
    template<class T, class... TS> void operator () (const char * name, const T & field, TS...) 
    { 
        std::cout << "Name: " << name << std::endl;
    } 
};

template<class T> void serialize_test(const T & o) 
{ 
    uint32_t test;
    visit_fields(const_cast<T &>(o), field_encoder{ }); 
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
