#pragma once

#ifndef vr_material_hpp
#define vr_material_hpp

#include "GL_API.hpp"
#include "linalg_util.hpp"
#include "geometric.hpp"
#include "assets.hpp"

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

        MetallicRoughnessMaterial(std::shared_ptr<GlShader> shader)
        {
            program = shader;
        }

        virtual void update_uniforms() override
        {
            program->bind();

            //program->uniform("u_roughness", roughnessFactor);
            //program->uniform("u_metallic", metallicFactor);
            //program->uniform("u_ambientIntensity", ambientIntensity);

            program->texture("s_albedo", 0, albedo->asset, GL_TEXTURE_2D);
            program->texture("s_normal", 1, normal->asset, GL_TEXTURE_2D);
            program->texture("s_roughness", 2, roughness->asset, GL_TEXTURE_2D);
            program->texture("s_metallic", 3, metallic->asset, GL_TEXTURE_2D);

            program->texture("sc_radiance", 4, radianceCubemap->asset, GL_TEXTURE_CUBE_MAP);
            program->texture("sc_irradiance", 5, irradianceCubemap->asset, GL_TEXTURE_CUBE_MAP);

            //program->texture("s_emissive", 6, emissive, GL_TEXTURE_2D);
            //program->texture("s_occlusion", 7, occlusion, GL_TEXTURE_2D);

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
