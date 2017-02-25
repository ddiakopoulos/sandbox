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

	controllerRenderData = std::make_shared<OpenVR_Controller::ControllerRenderData>();
	controllers[0].renderData = controllerRenderData;
	controllers[1].renderData = controllerRenderData;

	renderModels = (vr::IVRRenderModels *)vr::VR_GetGenericInterface(vr::IVRRenderModels_Version, &eError);
	if (!renderModels)
	{
		vr::VR_Shutdown();
		throw std::runtime_error("Unable to get render model interface: " + std::string(vr::VR_GetVRInitErrorAsEnglishDescription(eError)));
	}

	{
		vr::RenderModel_t * model = nullptr;
		vr::RenderModel_TextureMap_t * texture = nullptr;

		while (true)
		{
			// see VREvent_TrackedDeviceActivated below for the proper way of doing this
			renderModels->LoadRenderModel_Async("vr_controller_vive_1_5", &model);
			if (model) renderModels->LoadTexture_Async(model->diffuseTextureId, &texture);
			if (model && texture) break;
		}

		controllerRenderData->mesh.set_vertex_data(sizeof(vr::RenderModel_Vertex_t)*model->unVertexCount, model->rVertexData, GL_STATIC_DRAW);
		controllerRenderData->mesh.set_attribute(0, 3, GL_FLOAT, GL_FALSE, sizeof(vr::RenderModel_Vertex_t), (const GLvoid *)offsetof(vr::RenderModel_Vertex_t, vPosition));
		controllerRenderData->mesh.set_attribute(1, 3, GL_FLOAT, GL_FALSE, sizeof(vr::RenderModel_Vertex_t), (const GLvoid *)offsetof(vr::RenderModel_Vertex_t, vNormal));
		controllerRenderData->mesh.set_attribute(3, 2, GL_FLOAT, GL_FALSE, sizeof(vr::RenderModel_Vertex_t), (const GLvoid *)offsetof(vr::RenderModel_Vertex_t, rfTextureCoord));
		controllerRenderData->mesh.set_index_data(GL_TRIANGLES, GL_UNSIGNED_SHORT, model->unTriangleCount * 3, model->rIndexData, GL_STATIC_DRAW);

		glTextureImage2DEXT(controllerRenderData->tex, GL_TEXTURE_2D, 0, GL_RGBA, texture->unWidth, texture->unHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, texture->rubTextureMapData);
		glGenerateTextureMipmapEXT(controllerRenderData->tex, GL_TEXTURE_2D);
		glTextureParameteriEXT(controllerRenderData->tex, GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTextureParameteriEXT(controllerRenderData->tex, GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

		for (uint32_t i = 0; i<model->unVertexCount; ++i) controllerRenderData->verts.push_back(reinterpret_cast<const float3 &>(model->rVertexData[i].vPosition));

		renderModels->FreeTexture(texture);
		renderModels->FreeRenderModel(model);

		controllerRenderData->loaded = true;
	}

	// Setup Framebuffers
	hmd->GetRecommendedRenderTargetSize(&renderTargetSize.x, &renderTargetSize.y);


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
		
		case vr::VREvent_TrackedDeviceActivated: 
		{
			std::cout << "Device " << event.trackedDeviceIndex << " attached." << std::endl;

			if (hmd->GetTrackedDeviceClass(event.trackedDeviceIndex) == vr::TrackedDeviceClass_Controller && controllerRenderData->loaded == false)
			{ 
				vr::EVRInitError eError = vr::VRInitError_None;
				std::string sRenderModelName = get_tracked_device_string(hmd, event.trackedDeviceIndex, vr::Prop_RenderModelName_String);
				std::cout << "Render Model Is: " << sRenderModelName << std::endl;
			}

			break;
		}
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
					controllers[0].trigger.update(!!(controllerState.ulButtonPressed & vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Trigger)));
					controllers[0].pad.update(!!(controllerState.ulButtonPressed & vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Touchpad)));
					controllers[0].touchpad.x = controllerState.rAxis[vr::k_eControllerAxis_TrackPad].x;
					controllers[0].touchpad.y = controllerState.rAxis[vr::k_eControllerAxis_TrackPad].y;
					controllers[0].p = make_pose(poses[i].mDeviceToAbsoluteTracking);
				}
				break;
			}
			case vr::TrackedControllerRole_RightHand:
			{
				if (hmd->GetControllerState(i, &controllerState))
				{
					controllers[1].trigger.update(!!(controllerState.ulButtonPressed & vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Trigger)));
					controllers[1].pad.update(!!(controllerState.ulButtonPressed & vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Touchpad)));
					controllers[1].touchpad.x = controllerState.rAxis[vr::k_eControllerAxis_TrackPad].x;
					controllers[1].touchpad.y = controllerState.rAxis[vr::k_eControllerAxis_TrackPad].y;
					controllers[1].p = make_pose(poses[i].mDeviceToAbsoluteTracking);
				}
				break;
			}
			}
			break;
		}
		}
	}
}

void OpenVR_HMD::submit(const GlTexture2D & leftEye, const GlTexture2D & rightEye)
{
	const vr::Texture_t leftTex = { (void*)(intptr_t)(GLuint)leftEye, vr::API_OpenGL, vr::ColorSpace_Gamma };
	vr::VRCompositor()->Submit(vr::Eye_Left, &leftTex);

	const vr::Texture_t rightTex = { (void*)(intptr_t)(GLuint)rightEye, vr::API_OpenGL, vr::ColorSpace_Gamma };
	vr::VRCompositor()->Submit(vr::Eye_Right, &rightTex);

	glFlush();
}