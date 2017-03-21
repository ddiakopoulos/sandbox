#pragma once

#ifndef vr_material_hpp
#define vr_material_hpp

#include "GL_API.hpp"
#include "linalg_util.hpp"
#include "geometric.hpp"
#include <memory>

namespace avl
{

    struct Material
    {
        std::shared_ptr<GlShader> program;
        virtual void update_uniforms() {}
        virtual void use(const float4x4 & modelMatrix) {}
    };

    class DebugMaterial : public Material
    {
    public:
        DebugMaterial(std::shared_ptr<GlShader> shader)
        {
            program = shader;
        }

        virtual void use(const float4x4 & modelMatrix) override
        {
            program->bind();
            program->uniform("u_modelMatrix", modelMatrix);
            program->uniform("u_modelMatrixIT", inv(transpose(modelMatrix)));
        }
    };

    class WireframeMaterial : public Material
    {
    public:
        WireframeMaterial(std::shared_ptr<GlShader> shader)
        {
            program = shader;
        }

        virtual void use(const float4x4 & modelMatrix) override
        {
            program->bind();
            program->uniform("u_modelMatrix", modelMatrix);
        }
    };

    class TexturedMaterial : public Material
    {

        GlTexture2D diffuseTexture;

    public:

        TexturedMaterial(std::shared_ptr<GlShader> shader)
        {
            program = shader;
        }

        virtual void update_uniforms() override
        {
            program->bind();

            program->uniform("u_ambientLight", float3(1.0f, 1.0f, 1.0f));

            program->uniform("u_rimLight.enable", 1);

            program->uniform("u_material.diffuseIntensity", float3(1.0f, 1.0f, 1.0f));
            program->uniform("u_material.ambientIntensity", float3(1.0f, 1.0f, 1.0f));
            program->uniform("u_material.specularIntensity", float3(1.0f, 1.0f, 1.0f));
            program->uniform("u_material.specularPower", 128.0f);

            program->uniform("u_pointLights[0].position", float3(6, 10, -6));
            program->uniform("u_pointLights[0].diffuseColor", float3(1.f, 0.0f, 0.0f));
            program->uniform("u_pointLights[0].specularColor", float3(1.f, 1.0f, 1.0f));

            program->uniform("u_pointLights[1].position", float3(-6, 10, 6));
            program->uniform("u_pointLights[1].diffuseColor", float3(0.0f, 0.0f, 1.f));
            program->uniform("u_pointLights[1].specularColor", float3(1.0f, 1.0f, 1.f));

            program->uniform("u_enableDiffuseTex", 1);
            program->uniform("u_enableNormalTex", 0);
            program->uniform("u_enableSpecularTex", 0);
            program->uniform("u_enableEmissiveTex", 0);
            program->uniform("u_enableGlossTex", 0);

            program->texture("u_diffuseTex", 0, diffuseTexture, GL_TEXTURE_2D);

            program->unbind();
        }

        virtual void use(const float4x4 & modelMatrix) override
        {
            program->bind();
            program->uniform("u_modelMatrix", modelMatrix);
            program->uniform("u_modelMatrixIT", inv(transpose(modelMatrix)));
        }

        void set_diffuse_texture(GlTexture2D & tex)
        {
            diffuseTexture = std::move(tex);
        }
    };

    class MetallicRoughnessMaterial : public Material
    {

        GlTexture2D albedo;
        GlTexture2D normal;
        GlTexture2D metallic;
        GlTexture2D roughness;
        GlTexture2D emissive;
        GlTexture2D occlusion;

        float roughnessFactor{ 1.f };
        float metallicFactor{ 1.f };
        float specularFactor{ 1.f };

        float3 emissiveColor{ float3(1, 1, 1) };
        float emissiveStrength{ 1.f };

        GlTextureObject radianceCubemap;
        GlTextureObject irradianceCubemap;

    public:

        MetallicRoughnessMaterial(std::shared_ptr<GlShader> shader)
        {
            program = shader;
        }

        virtual void update_uniforms() override
        {
            program->bind();

            //program->uniform("u_roughness", roughnessFactor);
            //program->uniform("u_metallic", metallicFactor);
            //program->uniform("u_specular", specularFactor);

            program->texture("s_albedo", 0, albedo, GL_TEXTURE_2D);
            program->texture("s_normal", 1, normal, GL_TEXTURE_2D);
            program->texture("s_roughness", 2, roughness, GL_TEXTURE_2D);
            program->texture("s_metallic", 3, metallic, GL_TEXTURE_2D);

            program->texture("sc_radiance", 4, radianceCubemap, GL_TEXTURE_CUBE_MAP);
            program->texture("sc_irradiance", 5, irradianceCubemap, GL_TEXTURE_CUBE_MAP);

            //program->texture("s_emissive", 4, emissive, GL_TEXTURE_2D);
            //program->texture("s_occlusion", 5, occlusion, GL_TEXTURE_2D);

            program->unbind();
        }

        virtual void use(const float4x4 & modelMatrix) override
        {
            program->bind();

            program->uniform("u_modelMatrix", modelMatrix);
            program->uniform("u_modelMatrixIT", inv(transpose(modelMatrix)));
        }

        void set_emissive_strength(const float & strength) { emissiveStrength = strength; }
        void set_emissive_color(const float3 & color) { emissiveColor = color; }

        void set_roughness(const float & term) { roughnessFactor = term; }
        void set_metallic(const float & term) { metallicFactor = term; }
        void set_specular(const float & term) { specularFactor = term; }

        void set_albedo_texture(GlTexture2D & tex) { albedo = std::move(tex); }
        void set_normal_texture(GlTexture2D & tex) { normal = std::move(tex); }
        void set_metallic_texture(GlTexture2D & tex) { metallic = std::move(tex); }
        void set_roughness_texture(GlTexture2D & tex) { roughness = std::move(tex); }
        void set_emissive_texture(GlTexture2D & tex) { emissive = std::move(tex); }
        void set_occulusion_texture(GlTexture2D & tex) { occlusion = std::move(tex); }
        void set_radiance_cubemap(GlTextureObject & cubemap) { radianceCubemap = std::move(cubemap); }
        void set_irrradiance_cubemap(GlTextureObject & cubemap) { irradianceCubemap = std::move(cubemap); }
    };

}

#endif // end vr_material_hpp
