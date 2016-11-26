#pragma once

#ifndef vr_hmd_hpp
#define vr_hmd_hpp

#include "openvr/include/openvr.h"
#include "linalg_util.hpp"
#include "geometric.hpp"
#include "geometry.hpp"

using namespace avl;

inline Pose make_pose(const vr::HmdMatrix34_t & m)
{
	return {
		make_rotation_quat_from_rotation_matrix({ { m.m[0][0], m.m[1][0], m.m[2][0] },{ m.m[0][1], m.m[1][1], m.m[2][1] },{ m.m[0][2], m.m[1][2], m.m[2][2] } }),
		{ m.m[0][3], m.m[1][3], m.m[2][3] }
	};
}

struct ControllerRenderData
{
	GlMesh mesh;
	std::vector<float3> verts;
	GlTexture2D tex;
};

struct Controller
{
	struct ButtonState
	{
		bool down = false;
		bool lastDown = false;
		bool pressed = false;
		bool released = false;

		void update(bool state)
		{
			lastDown = down;
			down = state;
			pressed = (!lastDown) && state;
			released = lastDown && (!state);
		}
	};

	ButtonState pad;
	ButtonState trigger;
	float2 touchpad = float2(0.0f, 0.0f);

	Pose p;

	Ray forward_ray() const { return Ray(p.position, p.transform_vector(float3(0.0f, 0.0f, -1.0f))); }

	std::shared_ptr<ControllerRenderData> renderData;
};

class OpenVR_HMD
{
	vr::IVRSystem * hmd = nullptr;
	vr::IVRRenderModels * renderModels = nullptr;

	uint2 renderTargetSize;
	Pose hmdPose;
	Pose worldPose;

	GlFramebuffer eyeFramebuffers[2];
	GlTexture2D eyeTextures[2];
	GlRenderbuffer multisampleRenderbuffers[2];
	GlFramebuffer multisampleFramebuffer;

	std::shared_ptr<ControllerRenderData> controllerRenderData;

	Controller controllers[2];

public:

	OpenVR_HMD();
	~OpenVR_HMD();

	const Controller & get_controller(const vr::ETrackedControllerRole controller) const
	{
		if (controller == vr::TrackedControllerRole_LeftHand) return controllers[0];
		if (controller == vr::TrackedControllerRole_RightHand) return controllers[1];
	}

	std::shared_ptr<ControllerRenderData> get_controller_render_data()
	{
		return controllerRenderData;
	}

	Pose get_hmd_pose() { return worldPose * hmdPose; }
	void set_hmd_pose(Pose p) { hmdPose = p; }

	GLuint get_eye_texture(vr::Hmd_Eye eye) const { return eyeTextures[eye]; }

	float4x4 get_proj_matrix(vr::Hmd_Eye eye, float near_clip, float far_clip) { return transpose(reinterpret_cast<const float4x4 &>(hmd->GetProjectionMatrix(eye, near_clip, far_clip, vr::API_OpenGL))); }

	Pose get_eye_pose(vr::Hmd_Eye eye) { return get_hmd_pose() * make_pose(hmd->GetEyeToHeadTransform(eye)); }

	void update();

	void render(float near, float far, std::function<void(Pose eye, float4x4 projMat)> renderFunc);
};

#endif