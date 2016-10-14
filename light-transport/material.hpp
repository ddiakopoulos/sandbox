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
// Wo = vector pointing in the opposite direction of the incident ray
// Wr = reflected vector
// Wt = transmitted vector
// We = emitted vector
// Wi = incident vector
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
	float3 Wr;
	float3 Wt;
	float brdf;
	float btdf = 0.f;
	float pdf;
	SurfaceScatterEvent(const IntersectionInfo * info) : info(info) {}
};

struct Material
{
	float3 Kd = { 0, 0, 0 }; // diffuse
	float3 Ke = { 0, 0, 0 }; // emissive

	virtual void sample(UniformRandomGenerator & gen, SurfaceScatterEvent & event) const = 0;

	// Evaluate the probability density function - p(x)
	virtual float pdf() const = 0;
};

struct IdealDiffuse : public Material
{
	virtual void sample(UniformRandomGenerator & gen, SurfaceScatterEvent & event) const override final
	{
		// sample_hemisphere(event.info->N, gen);//
		event.Wr = sample_cosine_hemisphere({ gen.random_float(), gen.random_float() }); // sample the normal vector on a hemi
		event.Wt = float3(); // no transmission
		event.brdf = float(ANVIL_INV_PI) * dot(event.Wr, event.info->N);
		event.pdf = pdf();
	}

	virtual float pdf() const override final
	{
		return 1.f / ANVIL_TWO_PI;
	}
};

struct IdealSpecular : public Material
{
	virtual void sample(UniformRandomGenerator & gen, SurfaceScatterEvent & event) const override final
	{
		const float roughness = 0.925;
		event.Wr = reflect(-event.info->Wo, event.info->N);
		event.Wr = normalize(float3(
			event.Wr.x + (gen.random_float() - 0.5f) * roughness,
			event.Wr.y + (gen.random_float() - 0.5f) * roughness,
			event.Wr.z + (gen.random_float() - 0.5f) * roughness));
		event.Wt = float3(); // no transmission
		event.brdf = 1.f;
		event.pdf = pdf();
	}

	virtual float pdf() const override final
	{
		return 1.f;
	}
};

#endif
