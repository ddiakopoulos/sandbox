#include "renderer.hpp"

Renderer::Renderer(float2 renderSize) : renderSize(renderSize)
{

}

Renderer::~Renderer()
{

}
void Renderer::set_debug_camera(GlCamera * debugCam)
{

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
	run_skybox_pass();

	run_forward_pass();

	if (renderWireframe) run_forward_wireframe_pass();

	if (renderShadows) run_shadow_pass();

	run_post_pass();
}
