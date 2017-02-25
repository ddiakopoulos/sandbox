#pragma once

#ifndef vr_renderer_hpp
#define vr_renderer_hpp

#include "linalg_util.hpp"
#include "geometric.hpp"
#include "geometry.hpp"
#include "camera.hpp"
#include "gpu_timer.hpp"

using namespace avl;

struct DirectionalLight
{
	float3 color;
	float3 direction;
	float size;

	DirectionalLight(const float3 dir, const float3 color, float size) : direction(dir), color(color), size(size) {}

	float4x4 get_view_proj_matrix(float3 eyePoint)
	{
		auto p = look_at_pose(eyePoint, eyePoint + -direction);
		const float halfSize = size * 0.5f;
		return mul(make_orthographic_matrix(-halfSize, halfSize, -halfSize, halfSize, -halfSize, halfSize), make_view_matrix_from_pose(p));
	}
};

struct SpotLight
{
	float3 color;
	float3 direction;

	float3 position;
	float cutoff;
	float3 attenuation; // constant, linear, quadratic

	SpotLight(const float3 pos, const float3 dir, const float3 color, float cut, float3 att) : position(pos), direction(dir), color(color), cutoff(cut), attenuation(att) {}

	float4x4 get_view_proj_matrix()
	{
		auto p = look_at_pose(position, position + direction);
		return mul(make_perspective_matrix(to_radians(cutoff * 2.0f), 1.0f, 0.1f, 1000.f), make_view_matrix_from_pose(p));
	}

	float get_cutoff() { return cosf(to_radians(cutoff)); }
};

struct PointLight
{
	float3 color;
	float3 position;
	float3 attenuation; // constant, linear, quadratic

	PointLight(const float3 pos, const float3 color, float3 att) : position(pos), color(color), attenuation(att) {}
};

struct RenderSet
{

};

struct LightSet
{
	DirectionalLight * directionalLight;
	std::vector<PointLight *> pointLights;
	std::vector<SpotLight *> spotLights;
};

class Renderer
{
	std::vector<RenderSet *> renderSets;
	std::vector<LightSet *> lightSets;

	GlCamera * debugCamera;

	float2 renderSize;

	void run_skybox_pass();
	void run_forward_pass();
	void run_forward_wireframe_pass();
	void run_shadow_pass();

	void run_post_pass();

	void run_bloom_pass();
	void run_reflection_pass();
	void run_ssao_pass();
	void run_smaa_pass();
	void run_blackout_pass();

	GlGpuTimer renderTimer;

public:

	Renderer(float2 renderSize);
	~Renderer();

	void set_debug_camera(GlCamera * debugCam);
	GlCamera * get_debug_camera();

	void render_frame();

};

#endif // end vr_renderer_hpp
