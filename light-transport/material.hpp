#ifndef material_hpp
#define material_hpp

#pragma once

#include "geometric.hpp"
#include "util.hpp"
#include "light-transport/sampling.hpp"

// BRDF Lexicon
// ============================================================================
// P  = point of ray intersection
// N  = surface normal at P
// Wi = incident vector
// Wo = vector pointing in the opposite direction of the incident ray
// Wr = reflected vector
// Wt = transmitted vector
// We = emitted vector
// Le = emitted light
// ============================================================================

struct IntersectionInfo
{
	float3 Wo;
	float3 P;
	float3 N;
};

struct SurfaceScatterEvent 
{
	const IntersectionInfo * info;
	float3 Wi;
	float pdf;
	SurfaceScatterEvent(const IntersectionInfo * info) : info(info) {}
};

struct Material
{
	float3 Kd = { 0, 0, 0 }; // diffuse color
	virtual float3 sample(UniformRandomGenerator & gen, SurfaceScatterEvent & event) const = 0;
	virtual float eval(const float3 & Wo, const float3 & Wi) const = 0;
	virtual float3 eval(const float3 & Wo, const float3 & Wi, SurfaceScatterEvent & event) const { return Kd * eval(Wo, Wi); }
};

struct IdealDiffuse : public Material
{
	virtual float3 sample(UniformRandomGenerator & gen, SurfaceScatterEvent & event) const override final
	{
		event.Wi = cosine_hemisphere({ gen.random_float(), gen.random_float() }); // sample
		event.pdf = cosine_hemisphere_pdf(event.Wi);
		return Kd * eval(event.info->Wo, event.Wi); // evaulate the brdf
	}

	virtual float eval(const float3 & Wo, const float3 & Wi) const override final
	{
		return ANVIL_INV_PI;
	}
};

struct IdealSpecular : public Material
{
	virtual float3 sample(UniformRandomGenerator & gen, SurfaceScatterEvent & event) const override final
	{
		const float roughness = 0.925;
		event.Wi = reflect(-event.info->Wo, event.info->N);
		event.Wi = normalize(float3(
			event.Wi.x + (gen.random_float() - 0.5f) * roughness,
			event.Wi.y + (gen.random_float() - 0.5f) * roughness,
			event.Wi.z + (gen.random_float() - 0.5f) * roughness));
		event.pdf = 1.0f;
		return Kd * eval(event.info->Wo, event.Wi); // evaulate the brdf
	}

	virtual float eval(const float3 & Wo, const float3 & Wi) const override final
	{
		return 1.0f;;
	}
};

#endif
