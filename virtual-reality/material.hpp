#pragma once

#ifndef vr_material_hpp
#define vr_material_hpp

#include "GL_API.hpp"
#include "linalg_util.hpp"
#include "geometric.hpp"


using namespace avl;

struct Material
{
	std::shared_ptr<GlShader> program;
	virtual void update_uniforms() {}
	virtual void use(const float4x4 & modelMatrix) {}
};

struct DebugMaterial : Material
{
	DebugMaterial()
	{

	}
	virtual void update_uniforms() override 
	{
		program->bind();

		program->unbind();
	}

	virtual void use(const float4x4 & modelMatrix) override
	{
		program->uniform("u_modelMatrix", modelMatrix);
		program->uniform("u_modelMatrixIT", inv(transpose(modelMatrix)));
	}
};

#endif // end vr_material_hpp
