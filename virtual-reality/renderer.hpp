#pragma once

#ifndef vr_renderer_hpp
#define vr_renderer_hpp

#include "linalg_util.hpp"
#include "geometric.hpp"
#include "geometry.hpp"
#include "camera.hpp"

using namespace avl;

struct RenderSet
{

};

class Renderer
{
	std::vector<RenderSet *> renderSets;
	GlCamera * firstPersonCamera;
	GlCamera * debugCamera;

public:

	Renderer();
	~Renderer();

	void set_cameras(GlCamera * fpsCam, GlCamera * debugCam);
	GlCamera * get_active_camera();




};

#endif // end vr_renderer_hpp
