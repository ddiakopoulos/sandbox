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

void MetallicRoughnessMaterial::update_uniforms()
{
    bindpoint = 0;

    auto & shader = program.get();
    shader.bind();

    shader.uniform("u_roughness", roughnessFactor);
    shader.uniform("u_metallic", metallicFactor);

    shader.texture("s_albedo", bindpoint++, albedo.get(), GL_TEXTURE_2D);
    shader.texture("s_normal", bindpoint++, normal.get(), GL_TEXTURE_2D);
    shader.texture("s_roughness", bindpoint++, roughness.get(), GL_TEXTURE_2D);
    shader.texture("s_metallic", bindpoint++, metallic.get(), GL_TEXTURE_2D);
    
    shader.texture("sc_radiance", bindpoint++, radianceCubemap.get(), GL_TEXTURE_CUBE_MAP);
    shader.texture("sc_irradiance", bindpoint++, irradianceCubemap.get(), GL_TEXTURE_CUBE_MAP);

    if (shader.has_define("HAS_EMISSIVE_MAP")) shader.texture("s_emissive", bindpoint++, emissive.get(), GL_TEXTURE_2D);
    if (shader.has_define("HAS_HEIGHT_MAP")) shader.texture("s_height", bindpoint++, height.get(), GL_TEXTURE_2D);
    if (shader.has_define("HAS_OCCLUSION_MAP")) shader.texture("s_occlusion", bindpoint++, occlusion.get(), GL_TEXTURE_2D);
}

void MetallicRoughnessMaterial::update_cascaded_shadow_array_handle(GLuint handle)
{
    auto & shader = program.get();
    shader.bind();
    shader.texture("s_csmArray", bindpoint++, handle, GL_TEXTURE_2D_ARRAY);
}

void MetallicRoughnessMaterial::use()
{
    auto & shader = program.get();
    shader.bind();
}
