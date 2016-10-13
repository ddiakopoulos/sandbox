#ifndef material_hpp
#define material_hpp

#pragma once

#include "geometric.hpp"
#include "util.hpp"

inline float3 reflect(const float3 & I, const float3 & N)
{
	return I - (N * dot(N, I) * 2.f);
}

inline float3 refract(const float3 & I, const float3 & N, float eta)
{
	float k = 1.0f - eta * eta * (1.0f - dot(N, I) * dot(N, I));
	if (k < 0.0f) return float3();
	else return eta * I - (eta * dot(N, I) + std::sqrt(k)) * N;
}

inline float3 sample_hemisphere(const float3 & N, UniformRandomGenerator & gen)
{
	float r1 = gen.random_float_sphere();
	float r2 = gen.random_float();
	float r2s = std::sqrt(r2);

	float3 w = N;
	float3 u = normalize((cross((std::abs(w.x) > 0.1f ? float3(0, 1, 0) : float3(1, 1, 1)), w))); // u is perpendicular to w
	float3 v = cross(w, u); // v is perpendicular to u and w

	return normalize(u * std::cos(r1) * r2s + v * std::sin(r1) * r2s + w * std::sqrt(1.0f - r2));
}

// BRDF Lexicon
// ============================================================================
// P  = point of ray intersection
// N  = surface normal at P
// Wo = vector pointing in the opposite direction of the incident ray
// Wr = reflected vector
// Wt = transmitted vector
// We = emitted vector
// Wi = incident vector
// ============================================================================

struct BSDFResult
{
	float brdf;
	float btdf;
};

struct WiResult
{
	float3 Wr;
	float3 Wt;
};

struct Material
{
	float3 Kd = { 0, 0, 0 }; // diffuse
	float3 Ke = { 0, 0, 0 }; // emissive

	// Evaluate a normal and produce a reflected vector
	virtual WiResult evaulate_Wi(const float3 & Wo, const float3 & N, UniformRandomGenerator & gen) const { return WiResult();  }

	// Reflected
	virtual BSDFResult bsdf_Wr(const float3 & P, const float3 & N, const float3 & Wr, const float3 & Wt, const float3 & Wo, UniformRandomGenerator & gen) const { return BSDFResult(); }

	// Emitted
	virtual BSDFResult bsdf_We(const float3 & P, const float3 & N, const float3 & We, const float3 & Wr, const float3 & Wt, const float3 & Wo, UniformRandomGenerator & gen) const { return BSDFResult(); }

	// Evaluate the probability density function - p(x)
	virtual float pdf() const { return 0.f; }
};

struct IdealDiffuse : public Material
{
	virtual WiResult evaulate_Wi(const float3 & Wo, const float3 & N, UniformRandomGenerator & gen) const override final
	{
		return { sample_hemisphere(N, gen), float3(0.f) }; // sample the normal vector on a hemi, no transmission
	}

	BSDFResult bsdf_Wr(const float3 & P, const float3 & N, const float3 & Wr, const float3 & Wt, const float3 & Wo, UniformRandomGenerator & gen) const override final
	{
		return { float(ANVIL_INV_PI) * dot(Wr, N), 0.f };
	}

	BSDFResult bsdf_We(const float3 & P, const float3 & N, const float3 & We, const float3 & Wr, const float3 & Wt, const float3 & Wo, UniformRandomGenerator & gen) const override final
	{
		return { float(ANVIL_INV_PI) * std::max(dot(We, N), 0.f), 0.f };
	}

	float pdf() const
	{
		return 1.f / ANVIL_TWO_PI;
	}
};

#endif
