#ifndef lights_hpp
#define lights_hpp

#pragma once

#include "geometric.hpp"
#include "util.hpp"
#include "light-transport/sampling.hpp"

struct Light
{
	int numSamples = 2;
	virtual float3 sample(UniformRandomGenerator & gen, const float3 & P, float3 & Wi, float & pdf) const = 0;
	virtual float3 power() const = 0;
};

struct PointLight : public Light
{
	float3 intensity = { 1.f, 1.f, 1.f };
	float3 lightPos = { 0.f, 0.f, 0.f };
	virtual float3 sample(UniformRandomGenerator & gen, const float3 & P, float3 & Wi, float & pdf) const override final
	{
		Wi = normalize(lightPos - P) * uniform_sphere({ gen.random_float(), gen.random_float() }); // comment the sphere sample for hard shadows
		pdf = 1.f; 
		return intensity / distance2(lightPos, P);
	}
	virtual float3 power() const override final
	{
		return float3(4.f  * intensity * float(ANVIL_PI));
	}
};

struct SpotLight : public Light
{
	virtual float3 sample(UniformRandomGenerator & gen, const float3 & P, float3 & Wi, float & pdf) const override final
	{
		float3(1.f);
	}
	virtual float3 power() const override final
	{
		return float3(1.f);
	}
};

struct DirectionalLight : public Light
{
	virtual float3 sample(UniformRandomGenerator & gen, const float3 & P, float3 & Wi, float & pdf) const override final
	{
		float3(1.f);
	}
	virtual float3 power() const override final
	{
		return float3(1.f);
	}
};

struct AreaLight : public Light
{
	virtual float3 sample(UniformRandomGenerator & gen, const float3 & P, float3 & Wi, float & pdf) const override final
	{
		float3(1.f);
	}
	virtual float3 power() const override final
	{
		return float3(1.f);
	}
};

#endif
