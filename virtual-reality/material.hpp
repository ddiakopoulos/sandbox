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

}

#endif // end vr_material_hpp
