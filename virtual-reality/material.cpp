#include "material.hpp"
#include "vr_renderer.hpp"

using namespace avl;

////////////////////////
//   Debug Material   //
////////////////////////

DebugMaterial::DebugMaterial(GlShaderHandle shader)
{
    program = shader;
}

void DebugMaterial::use()
{
    auto & shader = program.get();
    shader.bind();
}

//////////////////////////////////////////////////////
//   Physically-Based Metallic-Roughness Material   //
//////////////////////////////////////////////////////

MetallicRoughnessMaterial::MetallicRoughnessMaterial(GlShaderHandle shader)
{
    program = shader;
}

void MetallicRoughnessMaterial::update_uniforms(const RenderPassData * data)
{
    auto & shader = program.get();
    shader.bind();

    shader.texture("s_albedo", 0, albedo.get(), GL_TEXTURE_2D);
    shader.texture("s_normal", 1, normal.get(), GL_TEXTURE_2D);
    shader.texture("s_roughness", 2, roughness.get(), GL_TEXTURE_2D);
    shader.texture("s_metallic", 3, metallic.get(), GL_TEXTURE_2D);
            
    shader.texture("sc_radiance", 4, radianceCubemap.get(), GL_TEXTURE_CUBE_MAP);
    shader.texture("sc_irradiance", 5, irradianceCubemap.get(), GL_TEXTURE_CUBE_MAP);

    shader.uniform("u_roughness", roughnessFactor);
    shader.uniform("u_metallic", metallicFactor);

    // IBL 
    //shader.uniform("u_ambientIntensity", ambientIntensity);
    //program->texture("s_emissive", 6, emissive, GL_TEXTURE_2D);
    //program->texture("s_occlusion", 7, occlusion, GL_TEXTURE_2D);
}

void MetallicRoughnessMaterial::update_cascaded_shadow_array_handle(GLuint handle)
{
    auto & shader = program.get();
    shader.bind();
    shader.texture("s_csmArray", 6, handle, GL_TEXTURE_2D_ARRAY);
}

void MetallicRoughnessMaterial::use()
{
    auto & shader = program.get();
    shader.bind();
}
