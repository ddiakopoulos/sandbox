#pragma once

#ifndef vr_hmd_hpp
#define vr_hmd_hpp

#include "openvr/include/openvr.h"
#include "index.hpp"
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
	GlTexture tex;
};

struct Controller
{
	std::shared_ptr<ControllerRenderData> renderData;
	Pose p;
	bool trigger = false;
};

class OpenVR_HMD
{
	vr::IVRSystem * hmd = nullptr;
	vr::IVRRenderModels * renderModels = nullptr;

	uint2 renderTargetSize;
	Pose hmdPose;
	Pose worldPose;

	GlTexture eyeTextures[2];
	GlRenderbuffer multisampleRenderbuffers[2];
	GlFramebuffer multisampleFramebuffer;

	std::shared_ptr<ControllerRenderData> controllerRenderData;

public:

	Controller controllers[2];

	OpenVR_HMD();
	~OpenVR_HMD();

	Pose get_left_controller_pose() const { return worldPose * controllers[0].p; }
	Pose get_right_controller_pose() const { return worldPose * controllers[1].p; }

	Pose get_hmd_pose() { return worldPose * hmdPose; }
	void set_hmd_pose(Pose p) { hmdPose = p; }

	GLuint get_eye_texture(vr::Hmd_Eye eye) { return eyeTextures[eye].get_gl_handle(); }

	float4x4 get_proj_matrix(vr::Hmd_Eye eye, float near_clip, float far_clip) { return transpose(reinterpret_cast<const float4x4 &>(hmd->GetProjectionMatrix(eye, near_clip, far_clip, vr::API_OpenGL))); }

	Pose get_eye_pose(vr::Hmd_Eye eye) { return get_hmd_pose() * make_pose(hmd->GetEyeToHeadTransform(eye)); }

	void update();

	void render(float near, float far, std::function<void(Pose eye, float4x4 projMat)> renderFunc);
};

#endif
