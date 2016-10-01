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
	virtual Bounds3D world_bounds() const { return Bounds3D(); }; // FIXME: this will return local bounds for the time being
};

struct RaytracedSphere : public Sphere, public Traceable
{
	virtual RayIntersection intersects(const Ray & ray) override final
	{
		float outT;
		float3 outNormal;
		if (intersect_ray_sphere(ray, *this, &outT, &outNormal)) return RayIntersection(outT, outNormal, &m);
		else return RayIntersection();
	}
	virtual Bounds3D world_bounds() const override final
	{
		return Bounds3D(center - radius, center + radius);
	}
};

struct RaytracedMesh : public Traceable
{
	Geometry g;
	Bounds3D bounds;

	RaytracedMesh(Geometry & g) : g(std::move(g))
	{
		bounds = g.compute_bounds();
	}

	virtual RayIntersection intersects(const Ray & ray) override final
	{
		float outT;
		float3 outNormal;
		// intersect_ray_mesh() takes care of early out using bounding box & rays from inside
		if (intersect_ray_mesh(ray, g, &outT, &outNormal, &bounds)) return RayIntersection(outT, outNormal, &m);
		else return RayIntersection();
	}

	virtual Bounds3D world_bounds() const override final
	{
		return bounds;
	}
};


#endif
