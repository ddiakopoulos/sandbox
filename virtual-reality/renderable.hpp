#pragma once

#ifndef vr_renderable_hpp
#define vr_renderable_hpp

#include "linalg_util.hpp"
#include "geometric.hpp"
#include "geometry.hpp"
#include "camera.hpp"
#include "scene.hpp"

using namespace avl;

struct Renderable
{
	virtual void update(const float & dt) {}
	virtual void draw() const {};
	virtual Bounds3D get_bounds() const = 0;
	virtual float3 get_scale() const = 0;
	virtual Pose get_pose() const = 0;
	virtual void set_pose(const Pose & p) = 0;
	virtual RaycastResult raycast(const Ray & worldRay) const = 0;
};

#endif // end vr_renderable_hpp
