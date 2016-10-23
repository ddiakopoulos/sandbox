#ifndef lights_hpp
#define lights_hpp

#pragma once

#include "geometric.hpp"
#include "util.hpp"
#include "light-transport/sampling.hpp"
#include "light-transport/util.hpp"

struct Light
{
	int numSamples = 4;
	virtual float3 sample_direct(UniformRandomGenerator & gen, const float3 & P, float3 & Wi, float & pdf) const = 0;
};

struct PointLight : public Light
{
	float3 intensity = { 1.f, 1.f, 1.f };
	float3 lightPos = { 0.f, 0.f, 0.f };
	virtual float3 sample_direct(UniformRandomGenerator & gen, const float3 & P, float3 & Wi, float & pdf) const override final
	{
		Wi = normalize(lightPos - P) * uniform_sphere({ gen.random_float(), gen.random_float() }); // comment the sphere sample for hard shadows
		pdf = uniform_sphere_pdf();
		return intensity / distance2(lightPos, P);
	}
};

struct AreaLight : public Light
{
	Quad * quad;
	float3 intensity = { 1.f, 1.f, 1.f };

	virtual float3 sample_direct(UniformRandomGenerator & gen, const float3 & P, float3 & Wi, float & pdf) const override final
	{
		if (dot(quad->normal, P - quad->base) <= 0.0f)
			return float3(0, 0, 0);

		const float2 xi = { gen.random_float(), gen.random_float() };
		const float3 q = quad->base + xi.x*quad->edge0 + xi.y*quad->edge1;
		float3 pt = q - P;

		float rSq = length2(pt);
		const float distance = std::sqrt(rSq);
		pt /= distance;

		const float cosTheta = -dot(quad->normal, pt);

		Wi = pt;
		pdf = rSq / (cosTheta * quad->area);

		//std::cout << "sd: " << Wi << std::endl;

		float3 weight = intensity * float(ANVIL_PI) * quad->area;
		return weight;

	}
};

#endif
