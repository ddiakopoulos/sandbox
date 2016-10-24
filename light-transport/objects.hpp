#ifndef objects_hpp
#define objects_hpp

#pragma once

#include "geometric.hpp"
#include "geometry.hpp"
#include "light-transport/bsdf.hpp"
#include "light-transport/util.hpp"

using namespace avl;

struct Quad
{
	float3 base;
	float3 edge0, edge1;
	float2 invUvSq;
	float3 normal;
	float area;
	float invArea;
	float4x4 transform;

	Quad(float3 b, float3 e0, float3 e1) : base(b), edge0(e0), edge1(e1)
	{
		float4x4 t(float4(edge0, 0), float4(cross(edge1, edge0), 0), float4(edge1, 0), float4(0, 0, 0, 1));
		transform = mul(make_translation_matrix(base + edge0 * 0.5f + edge1 * 0.5f), t);

		// Prepare for render... 
		base = transform_coord(transform, float3(0, 0, 0));
		edge0 = transform_vector(transform, float3(1, 0, 0));
		edge1 = transform_vector(transform, float3(0, 0, 1));
		base -= edge0 * 0.5f;
		base -= edge1 * 0.5f;

		normal = cross(edge1, edge0);
		area = length(normal);
		invArea = 1.0f / area;
		normal /= area;

		invUvSq = 1.0f / float2(length2(edge0), length2(edge1));
	}
};

bool intersect_ray_quad(const Ray & ray, const Quad & q, float & outT, float3 & P)
{
	float nDotW = dot(q.normal, ray.direction);

	if (std::abs(nDotW) < 1e-6f)
		return false;

	float t = dot(q.normal, q.base - ray.origin) / nDotW;

	float3 p = ray.origin + t * ray.direction;
	float3 v = p - q.base;

	float l0 = dot(v, q.edge0) * q.invUvSq.x;
	float l1 = dot(v, q.edge1) * q.invUvSq.y;

	if (l0 < 0.0f || l0 > 1.0f || l1 < 0.0f || l1 > 1.0f)
	{
		return false;
	}

	outT = t;
	P = p;

	// u = l0;
	// v = 1.0f - l1;
	return true;
}

// ---------------

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

struct RaytracedQuad : public Traceable
{
	std::unique_ptr<Quad> q;
	virtual RayIntersection intersects(const Ray & ray)  override final
	{
		float outT;
		float3 outIntersection;
		if (intersect_ray_quad(ray, *q.get(), outT, outIntersection))
		{
			return RayIntersection(outT, q.get()->normal, m.get());
		}
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
