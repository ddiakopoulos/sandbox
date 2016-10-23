#ifndef bsdf_hpp
#define bsdf_hpp

#pragma once

#include "geometric.hpp"
#include "util.hpp"
#include "light-transport/sampling.hpp"
#include "light-transport/util.hpp"

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

const float glassAirIndexOfRefraction = 1.523f;
extern bool g_debug;

struct IntersectionInfo
{
	float3 Wo;
	float3 P;
	float3 N;
	float3 T;
	float3 BT;
	float3 Kd;
};

struct SurfaceScatterEvent
{
	const IntersectionInfo * info;
	float3 Wi;
	float pdf;
	SurfaceScatterEvent(const IntersectionInfo * info) : info(info) {}
};

struct BSDF
{
	float3 Kd = { 0, 0, 0 }; // diffuse color
	virtual float3 sample(UniformRandomGenerator & gen, SurfaceScatterEvent & event) const = 0;
	virtual float eval(const float3 & Wo, const float3 & Wi) const = 0;
	virtual float3 eval(const float3 & Wo, const float3 & Wi, SurfaceScatterEvent & event) const { return Kd * eval(Wo, Wi); }
};

struct IdealDiffuse : public BSDF
{
	virtual float3 sample(UniformRandomGenerator & gen, SurfaceScatterEvent & event) const override final
	{
		event.Wi = cosine_hemisphere({ gen.random_float(), gen.random_float() }); // sample
		event.pdf = cosine_hemisphere_pdf(event.Wi);
		return Kd * eval(event.info->Wo, event.Wi);
	}

	virtual float eval(const float3 & Wo, const float3 & Wi) const override final
	{
		return ANVIL_INV_PI;
	}
};

struct IdealSpecular : public BSDF
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
		return Kd * eval(event.info->Wo, event.Wi);
	}

	virtual float eval(const float3 & Wo, const float3 & Wi) const override final
	{
		return 1.0f;
	}
};

struct Mirror : public BSDF
{
	virtual float3 sample(UniformRandomGenerator & gen, SurfaceScatterEvent & event) const override final
	{
		event.Wi = float3(-event.info->Wo.x, -event.info->Wo.y, event.info->Wo.z);
		event.pdf = 1.f;
		return event.info->Kd / std::abs(event.Wi.z);
	}

	virtual float eval(const float3 & Wo, const float3 & Wi) const override final
	{
		if (reflection_constraint(Wi, Wo)) return 1.0f;
		else return 0.0f;
	}
};

struct DialectricBSDF : public BSDF
{
	float IoR = glassAirIndexOfRefraction;

	virtual float3 sample(UniformRandomGenerator & gen, SurfaceScatterEvent & event) const override final
	{
		// Entering the medium or leaving it? 
		bool entering = dot(event.info->N, event.info->Wo) > 0.0f;
		float3 normal = entering ? -event.info->N : event.info->N;

		// Calculate eta depending on situation
		float eta = entering ? IoR : (1.f / IoR);

		// Angle of refraction
		float CosThetaT = 0.0f;
		const float CosThetaI = dot(event.info->Wo, -normal);
		const float reflectance = dielectric_reflectance(eta, CosThetaI, CosThetaT);

		float3 weight;

		if (gen.random_float() < reflectance)
		{
			// Reflect
			event.Wi = float3(-event.info->Wo.x, -event.info->Wo.y, event.info->Wo.z);
			event.pdf = reflectance;
			weight = event.info->Kd * reflectance;
		}
		else
		{
			// Total internal reflection
			if (reflectance == 1.f) return float3(0, 0, 0);

			// Refract 
			event.Wi = normalize(float3(-event.info->Wo.x * eta, -event.info->Wo.y * eta, -std::copysign(CosThetaT, event.info->Wo.z)));
			event.pdf = 1.f - reflectance;
			float3 transmittance = event.info->Kd;
			weight = (1.f - reflectance) * transmittance;
		}

		return weight;
	}

	virtual float eval(const float3 & Wo, const float3 & Wi) const override final
	{
		if (reflection_constraint(Wi, Wo)) return 1.0f;
		else return 0.0f;
	}

};

#endif
