#include "material.hpp"
#include "vr_renderer.hpp"

using namespace avl;

////////////////////////
//   Debug Material   //
////////////////////////

DebugMaterial::DebugMaterial(std::shared_ptr<GlShader> shader)
{
    program = shader;
}

void DebugMaterial::use(const float4x4 & modelMatrix, const float4x4 & viewMatrix)
{
    program->bind();
    program->uniform("u_modelMatrix", modelMatrix);
    program->uniform("u_modelMatrixIT", inv(transpose(modelMatrix)));
}

////////////////////////////
//   Wireframe Material   //
////////////////////////////

WireframeMaterial::WireframeMaterial(std::shared_ptr<GlShader> shader)
{
    program = shader;
}

void WireframeMaterial::use(const float4x4 & modelMatrix, const float4x4 & viewMatrix) 
{
    program->bind();
    program->uniform("u_modelMatrix", modelMatrix);
}

///////////////////////////////////
//   Standard Diffuse Material   //
///////////////////////////////////

TexturedMaterial::TexturedMaterial(std::shared_ptr<GlShader> shader)
{
    program = shader;
}

void TexturedMaterial::update_uniforms(const RenderPassData * data)
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

void TexturedMaterial::use(const float4x4 & modelMatrix, const float4x4 & viewMatrix) 
{
    program->bind();
    program->uniform("u_modelMatrix", modelMatrix);
    program->uniform("u_modelMatrixIT", inv(transpose(modelMatrix)));
}

void TexturedMaterial::set_diffuse_texture(GlTexture2D & tex)
{
    diffuseTexture = std::move(tex);
}

//////////////////////////////////////////////////////
//   Physically-Based Metallic-Roughness Material   //
//////////////////////////////////////////////////////

MetallicRoughnessMaterial::MetallicRoughnessMaterial(std::shared_ptr<GlShader> shader)
{
    program = shader;
}

void MetallicRoughnessMaterial::update_uniforms(const RenderPassData * data)
{
    program->bind();

    program->texture("s_albedo", 0, albedo->asset, GL_TEXTURE_2D);
    program->texture("s_normal", 1, normal->asset, GL_TEXTURE_2D);
    program->texture("s_roughness", 2, roughness->asset, GL_TEXTURE_2D);
    program->texture("s_metallic", 3, metallic->asset, GL_TEXTURE_2D);
            
    program->texture("sc_radiance", 4, radianceCubemap->asset, GL_TEXTURE_CUBE_MAP);
    program->texture("sc_irradiance", 5, irradianceCubemap->asset, GL_TEXTURE_CUBE_MAP);

    if (data->shadow.csmArrayHandle)
    {
        program->texture("s_csmArray", 6, data->shadow.csmArrayHandle, GL_TEXTURE_2D_ARRAY);
    }

    //program->texture("s_emissive", 6, emissive, GL_TEXTURE_2D);
    //program->texture("s_occlusion", 7, occlusion, GL_TEXTURE_2D);
}

void MetallicRoughnessMaterial::use(const float4x4 & modelMatrix, const float4x4 & viewMatrix)
{
    program->bind();
    program->uniform("u_modelMatrix", modelMatrix);
    program->uniform("u_modelMatrixIT", inv(transpose(modelMatrix)));
    program->uniform("u_modelViewMatrix", mul(viewMatrix, modelMatrix));
    program->uniform("u_roughness", roughnessFactor);
    program->uniform("u_metallic", metallicFactor);
    program->uniform("u_ambientIntensity", ambientIntensity);
}
