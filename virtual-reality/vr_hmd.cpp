#include "vr_hmd.hpp"

std::string get_tracked_device_string(vr::IVRSystem * pHmd, vr::TrackedDeviceIndex_t unDevice, vr::TrackedDeviceProperty prop, vr::TrackedPropertyError *peError = NULL)
{
	uint32_t unRequiredBufferLen = pHmd->GetStringTrackedDeviceProperty(unDevice, prop, NULL, 0, peError);
	if (unRequiredBufferLen == 0) return "";
	std::vector<char> pchBuffer(unRequiredBufferLen);
	unRequiredBufferLen = pHmd->GetStringTrackedDeviceProperty(unDevice, prop, pchBuffer.data(), unRequiredBufferLen, peError);
	std::string result = { pchBuffer.begin(), pchBuffer.end() };
	return result;
}

OpenVR_HMD::OpenVR_HMD()
{
	vr::EVRInitError eError = vr::VRInitError_None;
	hmd = vr::VR_Init(&eError, vr::VRApplication_Scene);
	if (eError != vr::VRInitError_None) throw std::runtime_error("Unable to init VR runtime: " + std::string(vr::VR_GetVRInitErrorAsEnglishDescription(eError)));

	std::cout << "VR Driver:  " << get_tracked_device_string(hmd, vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_TrackingSystemName_String) << std::endl;
	std::cout << "VR Display: " << get_tracked_device_string(hmd, vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_SerialNumber_String) << std::endl;

	controllerRenderData = std::make_shared<ControllerRenderData>();
	controllers[0].renderData = controllerRenderData;
	controllers[1].renderData = controllerRenderData;

	renderModels = (vr::IVRRenderModels *)vr::VR_GetGenericInterface(vr::IVRRenderModels_Version, &eError);
	if (!renderModels)
	{
		vr::VR_Shutdown();
		throw std::runtime_error("Unable to get render model interface: " + std::string(vr::VR_GetVRInitErrorAsEnglishDescription(eError)));
	}

	vr::RenderModel_t * model = nullptr;
	vr::RenderModel_TextureMap_t * texture = nullptr;

	while (true)
	{
		renderModels->LoadRenderModel_Async("vr_controller_05_wireless_b", &model);
		if (model) renderModels->LoadTexture_Async(model->diffuseTextureId, &texture);
		if (model && texture) break;
	}

	controllerRenderData->mesh.set_vertex_data(sizeof(vr::RenderModel_Vertex_t)*model->unVertexCount, model->rVertexData, GL_STATIC_DRAW);
	controllerRenderData->mesh.set_attribute(0, 3, GL_FLOAT, GL_FALSE, sizeof(vr::RenderModel_Vertex_t), (const GLvoid *)offsetof(vr::RenderModel_Vertex_t, vPosition));
	controllerRenderData->mesh.set_attribute(1, 3, GL_FLOAT, GL_FALSE, sizeof(vr::RenderModel_Vertex_t), (const GLvoid *)offsetof(vr::RenderModel_Vertex_t, vNormal));
	controllerRenderData->mesh.set_attribute(3, 2, GL_FLOAT, GL_FALSE, sizeof(vr::RenderModel_Vertex_t), (const GLvoid *)offsetof(vr::RenderModel_Vertex_t, rfTextureCoord));
	controllerRenderData->mesh.set_index_data(GL_TRIANGLES, GL_UNSIGNED_SHORT, model->unTriangleCount * 3, model->rIndexData, GL_STATIC_DRAW);

	const auto ctlTexHandle = controllerRenderData->tex.get_gl_handle();
	glTextureImage2DEXT(ctlTexHandle, GL_TEXTURE_2D, 0, GL_RGBA, texture->unWidth, texture->unHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, texture->rubTextureMapData);
	glGenerateTextureMipmapEXT(ctlTexHandle, GL_TEXTURE_2D);
	glTextureParameteriEXT(ctlTexHandle, GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTextureParameteriEXT(ctlTexHandle, GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

	for (uint32_t i = 0; i<model->unVertexCount; ++i) controllerRenderData->verts.push_back(reinterpret_cast<const float3 &>(model->rVertexData[i].vPosition));
	
	renderModels->FreeTexture(texture);
	renderModels->FreeRenderModel(model);

	// Setup Framebuffers
	hmd->GetRecommendedRenderTargetSize(&renderTargetSize.x, &renderTargetSize.y);

	// Generate multisample render buffers for color and depth
	glNamedRenderbufferStorageMultisampleEXT(multisampleRenderbuffers[0].get_handle(), 4, GL_RGBA8, renderTargetSize.x, renderTargetSize.y);
	glNamedRenderbufferStorageMultisampleEXT(multisampleRenderbuffers[1].get_handle(), 4, GL_DEPTH_COMPONENT, renderTargetSize.x, renderTargetSize.y);

	// Generate a framebuffer for multisample rendering
	glNamedFramebufferRenderbufferEXT(multisampleFramebuffer.get_handle(), GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, multisampleRenderbuffers[0].get_handle());
	glNamedFramebufferRenderbufferEXT(multisampleFramebuffer.get_handle(), GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, multisampleRenderbuffers[1].get_handle());
	if (glCheckNamedFramebufferStatusEXT(multisampleFramebuffer.get_handle(), GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) throw std::runtime_error("Framebuffer incomplete!");

	// Generate textures and framebuffers for the left and right eye images
	for (int i : {0, 1})
	{
		const auto texHandle = eyeTextures[i].get_gl_handle();
		const auto fboHandle = eyeFramebuffers[i].get_handle();

		glTextureImage2DEXT(texHandle, GL_TEXTURE_2D, 0, GL_RGBA8, renderTargetSize.x, renderTargetSize.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
		glTextureParameteriEXT(texHandle, GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTextureParameteriEXT(texHandle, GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTextureParameteriEXT(texHandle, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTextureParameteriEXT(texHandle, GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTextureParameteriEXT(texHandle, GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

		glNamedFramebufferTexture2DEXT(eyeFramebuffers[i].get_handle(), GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texHandle, 0);
		if (glCheckNamedFramebufferStatusEXT(eyeFramebuffers[i].get_handle(), GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) throw std::runtime_error("Framebuffer incomplete!");
	}

	// Setup the compositor
	if (!vr::VRCompositor())
	{
		throw std::runtime_error("could not initialize VRCompositor");
	}
}

OpenVR_HMD::~OpenVR_HMD()
{
	glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_FALSE);
	glDebugMessageCallback(nullptr, nullptr);
	if (hmd) vr::VR_Shutdown();
}

void OpenVR_HMD::update()
{
	// Handle events
	vr::VREvent_t event;
	while (hmd->PollNextEvent(&event, sizeof(event)))
	{
		switch (event.eventType)
		{
		case vr::VREvent_TrackedDeviceActivated: std::cout << "Device " << event.trackedDeviceIndex << " attached." << std::endl; break;
		case vr::VREvent_TrackedDeviceDeactivated: std::cout << "Device " << event.trackedDeviceIndex << " detached." << std::endl; break;
		case vr::VREvent_TrackedDeviceUpdated: std::cout << "Device " << event.trackedDeviceIndex << " updated." << std::endl; break;
		}
	}

	// Get HMD pose
	std::array<vr::TrackedDevicePose_t, 16> poses;
	vr::VRCompositor()->WaitGetPoses(poses.data(), static_cast<uint32_t>(poses.size()), nullptr, 0);
	for (vr::TrackedDeviceIndex_t i = 0; i < poses.size(); ++i)
	{
		if (!poses[i].bPoseIsValid) continue;
		switch (hmd->GetTrackedDeviceClass(i))
		{
		case vr::TrackedDeviceClass_HMD:
		{
			hmdPose = make_pose(poses[i].mDeviceToAbsoluteTracking); 
			break;
		}
		case vr::TrackedDeviceClass_Controller:
		{
			vr::VRControllerState_t controllerState = vr::VRControllerState_t();
			switch (hmd->GetControllerRoleForTrackedDeviceIndex(i))
			{
			case vr::TrackedControllerRole_LeftHand:
			{
				if (hmd->GetControllerState(i, &controllerState))
				{
					controllers[0].trigger = !!(controllerState.ulButtonPressed & vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Trigger));
					controllers[0].p = make_pose(poses[i].mDeviceToAbsoluteTracking);
				}
				controllers[0].p = make_pose(poses[i].mDeviceToAbsoluteTracking);
				break;
			}
			case vr::TrackedControllerRole_RightHand:
			{
				if (hmd->GetControllerState(i, &controllerState))
				{
					controllers[1].trigger = !!(controllerState.ulButtonPressed & vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Trigger));
					controllers[1].p = make_pose(poses[i].mDeviceToAbsoluteTracking);
				}
				controllers[1].p = make_pose(poses[i].mDeviceToAbsoluteTracking);
				break;
			}
			}
			break;
		}
		}
	}
}

void OpenVR_HMD::render(float near, float far, std::function<void(::Pose eyePose, float4x4 projMatrix)> renderFunc)
{
	glViewport(0, 0, renderTargetSize.x, renderTargetSize.y);

	for (auto eye : { vr::Eye_Left, vr::Eye_Right })
	{
		// Render into multisample
		glEnable(GL_MULTISAMPLE);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, multisampleFramebuffer.get_handle());
		renderFunc(get_eye_pose(eye), get_proj_matrix(eye, near, far));
		glDisable(GL_MULTISAMPLE);

		// Resolve multisample
		glBindFramebuffer(GL_READ_FRAMEBUFFER, multisampleFramebuffer.get_handle());
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, eyeTextures[eye].get_gl_handle());
		glBlitFramebuffer(0, 0, renderTargetSize.x, renderTargetSize.y, 0, 0, renderTargetSize.x, renderTargetSize.y, GL_COLOR_BUFFER_BIT, GL_LINEAR);
	}

	// Submit
	for (auto eye : { vr::Eye_Left, vr::Eye_Right })
	{
		const vr::Texture_t texture = { (void*)(intptr_t)(GLuint) eyeTextures[eye].get_gl_handle(), vr::API_OpenGL, vr::ColorSpace_Gamma };
		vr::VRCompositor()->Submit(eye, &texture);
	}
}
