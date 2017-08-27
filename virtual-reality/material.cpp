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
            
    //program->texture("sc_radiance", 4, radianceCubemap->asset, GL_TEXTURE_CUBE_MAP);
    //program->texture("sc_irradiance", 5, irradianceCubemap->asset, GL_TEXTURE_CUBE_MAP);

    if (data->shadow.csmArrayHandle)
    {
        program->texture("s_csmArray", 6, data->shadow.csmArrayHandle, GL_TEXTURE_2D_ARRAY);
    }

    program->uniform("u_roughness", roughnessFactor);
    program->uniform("u_metallic", metallicFactor);
    program->uniform("u_ambientIntensity", ambientIntensity);

    //program->texture("s_emissive", 6, emissive, GL_TEXTURE_2D);
    //program->texture("s_occlusion", 7, occlusion, GL_TEXTURE_2D);
}

void MetallicRoughnessMaterial::use(const float4x4 & modelMatrix, const float4x4 & viewMatrix)
{
    program->bind();
    program->uniform("u_modelMatrix", modelMatrix);
    program->uniform("u_modelMatrixIT", inv(transpose(modelMatrix)));
    program->uniform("u_modelViewMatrix", mul(viewMatrix, modelMatrix));
}
