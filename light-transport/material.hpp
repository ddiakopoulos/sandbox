#ifndef material_hpp
#define material_hpp

#pragma once

#include "geometric.hpp"
#include "util.hpp"

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

static inline float3 sample_cosine_hemisphere(const float2 & xi)
{
	float phi = xi.x * ANVIL_TWO_PI;
	float r = std::sqrt(xi.y);

	return float3(
		std::cos(phi)*r,
		std::sin(phi)*r,
		std::sqrt(std::max(1.0f - xi.y, 0.0f))
	);
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

inline float3 uniform_sample_sphere(UniformRandomGenerator & gen)
{
	const float2 p(gen.random_float(), gen.random_float());
	float z = 1.f - 2.f * p.x;
	float r = std::sqrt(std::max(0.f, 1.f - z * z));
	float phi = 2.f * ANVIL_PI * p.y;
	return float3(r * std::cos(phi), r * std::sin(phi), z); // need to be normalized?
}

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

struct Light
{
	int numSamples = 2;
	// Returns Le, or total light emitted from the source.
	virtual float3 sample(UniformRandomGenerator & gen, const float3 & P, float3 & Wi, float & pdf) const = 0;
	// All lights must also be able to return their total emitted power
	virtual float3 power() const = 0;
};

struct PointLight : public Light
{
	float3 intensity = { 1.f, 1.f, 1.f };
	float3 lightPos = { 0.f, 0.f, 0.f };
	virtual float3 sample(UniformRandomGenerator & gen, const float3 & P, float3 & Wi, float & pdf) const override final
	{
		Wi = normalize(lightPos - P) * uniform_sample_sphere(gen); // comment the sphere sample for hard shadows
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
