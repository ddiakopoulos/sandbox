#pragma once

#ifndef vr_renderer_hpp
#define vr_renderer_hpp

#include "linalg_util.hpp"
#include "geometric.hpp"
#include "geometry.hpp"
#include "renderable.hpp"
#include "camera.hpp"
#include "gpu_timer.hpp"

using namespace avl;

#if defined(ANVIL_PLATFORM_WINDOWS)
	#define ALIGNED(n) __declspec(align(n))
#else
	#define ALIGNED(n) alignas(n)
#endif

namespace uniforms
{
	// Add resolution
	struct per_scene
	{
		static const int      binding = 0;
		float				  time;
	};

	struct per_view
	{
		static const int      binding = 1;
		ALIGNED(16) float4x4  view;
		ALIGNED(16) float4x4  viewProj;
		ALIGNED(16) float3    eyePos;
	};

	struct directional_light
	{
		ALIGNED(16) float3	  color;
		ALIGNED(16) float3	  direction;
		ALIGNED(16) float	  size;
	};

	struct point_light
	{
		ALIGNED(16) float3	  color;
		ALIGNED(16) float3	  position;
		ALIGNED(16) float3	  attenuation; // constant, linear, quadratic
	};

	struct spot_light
	{
		ALIGNED(16) float3	  color;
		ALIGNED(16) float3	  direction;
		ALIGNED(16) float3	  position;
		ALIGNED(16) float3	  attenuation; // constant, linear, quadratic
		ALIGNED(16) float	  cutoff;
	};
}

enum class Eye : int
{
	LeftEye = 0, 
	RightEye = 1
};

struct EyeData
{
	Pose pose;
	float4x4 projectionMatrix;
};

struct RenderSet
{
	std::vector<Renderable * > objects;
};

struct LightSet
{
	uniforms::directional_light * directionalLight;
	std::vector<uniforms::point_light *> pointLights;
	std::vector<uniforms::spot_light *> spotLights;
};

class Renderer
{
	std::vector<RenderSet *> renderSets;
	std::vector<LightSet *> lightSets;

	float2 renderSize;
	GlGpuTimer renderTimer;

	GlBuffer perScene;
	GlBuffer perView;

	EyeData eyes[2];

	GlFramebuffer eyeFramebuffers[2];
	GlTexture2D eyeTextures[2];
	GlRenderbuffer multisampleRenderbuffers[2];
	GlFramebuffer multisampleFramebuffer;

	void run_skybox_pass();
	void run_forward_pass();
	void run_forward_wireframe_pass();
	void run_shadow_pass();

	void run_bloom_pass();
	void run_reflection_pass();
	void run_ssao_pass();
	void run_smaa_pass();
	void run_blackout_pass();

	void run_post_pass();

	bool renderWireframe { false };
	bool renderShadows { false };
	bool renderPost { false };
	bool renderBloom { false };
	bool renderReflection { false };
	bool renderSSAO { false };
	bool renderSMAA { false };
	bool renderBlackout { false };

public:

	Renderer(float2 renderSize);
	~Renderer();

	void render_frame();

	void set_eye_data(const EyeData left, const EyeData right);

	GlTexture2D & get_eye_texture(Eye e) { return eyeTextures[(int)e]; }
};

#endif // end vr_renderer_hpp
