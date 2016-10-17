#ifndef objects_hpp
#define objects_hpp

#pragma once

#include "geometric.hpp"
#include "geometry.hpp"
#include "light-transport/bsdf.hpp"
#include "light-transport/util.hpp"

using namespace avl;

struct RayIntersection
{
	float d = std::numeric_limits<float>::infinity();
	float maxt = 128.f;
	float3 normal;
	BSDF * m = nullptr;
	RayIntersection() {}
	RayIntersection(float d, float3 normal, BSDF * m) : d(d), normal(normal), m(m) {}
	bool operator() (void) { return (d < std::numeric_limits<float>::infinity() && d < maxt); }
};

struct Traceable
{
	std::shared_ptr<BSDF> m;
	virtual RayIntersection intersects(const Ray & ray) { return RayIntersection(); };
	virtual Bounds3D world_bounds() const { return Bounds3D(); }; // FIXME: this will return local bounds for the time being
};

struct RaytracedPlane : public Plane, public Traceable
{
	virtual RayIntersection intersects(const Ray & ray)  override final
	{
		float outT;
		float3 outIntersection;
		if (intersect_ray_plane(ray, *this, &outIntersection, &outT)) return RayIntersection(outT, get_normal(), m.get());
		else return RayIntersection();
	}
	virtual Bounds3D world_bounds() const override final
	{
		return Bounds3D();
	}
};

struct RaytracedSphere : public Sphere, public Traceable
{
	virtual RayIntersection intersects(const Ray & ray) override final
	{
		float outT;
		float3 outNormal;
		if (intersect_ray_sphere(ray, *this, &outT, &outNormal)) return RayIntersection(outT, outNormal, m.get());
		else return RayIntersection();
	}
	virtual Bounds3D world_bounds() const override final
	{
		return Bounds3D(center - radius, center + radius);
	}
};

struct RaytracedBox : public Bounds3D, public Traceable
{
	virtual RayIntersection intersects(const Ray & ray) override final
	{
		float outTMin, outTMax;
		float3 outNormal;
		if (intersect_ray_box(ray, *this, &outTMin, &outTMax, &outNormal)) { return RayIntersection(outTMin, outNormal, m.get()); }
		else return RayIntersection();
	}
	virtual Bounds3D world_bounds() const override final
	{
		return *this;
	}
};

struct RaytracedMesh : public Traceable
{
	Geometry g;
	Bounds3D bounds;

	RaytracedMesh(Geometry & g) : g(g)
	{
		bounds = g.compute_bounds();
	}

	virtual RayIntersection intersects(const Ray & ray) override final
	{
		float outT;
		float3 outNormal;
		// intersect_ray_mesh() takes care of early out using bounding box & rays from inside
		if (intersect_ray_mesh(ray, g, &outT, &outNormal, &bounds)) return RayIntersection(outT, outNormal, m.get());
		else return RayIntersection();
	}

	virtual Bounds3D world_bounds() const override final
	{
		return bounds;
	}
};


#endif
