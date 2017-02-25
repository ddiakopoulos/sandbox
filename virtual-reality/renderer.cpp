#include "renderer.hpp"

Renderer::Renderer(float2 renderSize) : renderSize(renderSize)
{

}

Renderer::~Renderer()
{

}
void Renderer::set_debug_camera(GlCamera * cam)
{
	debugCamera = cam;
}

GlCamera * Renderer::get_debug_camera()
{
	return debugCamera;
}

void Renderer::run_skybox_pass()
{

}

void Renderer::run_forward_pass()
{

}

void Renderer::run_forward_wireframe_pass()
{

}

void Renderer::run_shadow_pass()
{

}

void Renderer::run_bloom_pass()
{

}

void Renderer::run_reflection_pass()
{

}

void Renderer::run_ssao_pass()
{

}

void Renderer::run_smaa_pass()
{

}

void Renderer::run_blackout_pass()
{

}

void Renderer::run_post_pass()
{
	if (!renderPost) return;

	if (renderBloom) run_bloom_pass();

	if (renderReflection) run_reflection_pass();

	if (renderSSAO) run_ssao_pass();

	if (renderSMAA) run_smaa_pass();

	if (renderBlackout) run_blackout_pass();
}

void Renderer::render_frame()
{
	glEnable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);

	const float4x4 projMatrix = debugCamera->get_projection_matrix(float(renderSize.x) / float(renderSize.y));
	const float4x4 viewMatrix = debugCamera->get_view_matrix();
	const float4x4 viewProjMatrix = mul(projMatrix, viewMatrix);

	uniforms::per_scene b = {};
	b.time = 0.0f;
	perScene.set_buffer_data(sizeof(b), &b, GL_STREAM_DRAW);

	glBindBufferBase(GL_UNIFORM_BUFFER, uniforms::per_scene::binding, perScene);
	glBindBufferBase(GL_UNIFORM_BUFFER, uniforms::per_view::binding, perView);

	for (auto eye : { CameraView::LeftEye, CameraView::RightEye })
	{
		run_skybox_pass();

		run_forward_pass();

		if (renderWireframe) run_forward_wireframe_pass();

		if (renderShadows) run_shadow_pass();

		run_post_pass();
	}
}
