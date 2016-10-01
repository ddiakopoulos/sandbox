#ifndef objects_hpp
#define objects_hpp

#pragma once

#include "geometric.hpp"

using namespace avl;

struct RayIntersection
{
	float d = std::numeric_limits<float>::infinity();
	float3 location, normal;
	Material * m = nullptr;
	RayIntersection() {}
	RayIntersection(float d, float3 normal, Material * m) : d(d), normal(normal), m(m) {}
	bool operator() (void) { return d < std::numeric_limits<float>::infinity(); }
};

struct Traceable
{
	Material m;
	virtual RayIntersection intersects(const Ray & ray) { return RayIntersection(); };
};

struct RaytracedPlane : public Plane, public Traceable
{
	virtual RayIntersection intersects(const Ray & ray) override final
	{
		float outT;
		float3 outNormal;
		if (intersect_ray_plane(ray, *this)) return RayIntersection(outT, outNormal, &m);
		else return RayIntersection(); // nothing
	}
};

struct RaytracedSphere : public Sphere, public Traceable
{
	virtual RayIntersection intersects(const Ray & ray) override final
	{
		float outT;
		float3 outNormal;
		if (intersect_ray_sphere(ray, *this, &outT, &outNormal)) return RayIntersection(outT, outNormal, &m);
		else return RayIntersection(); // nothing
	}
};

#endif
