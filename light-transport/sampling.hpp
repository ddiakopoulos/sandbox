#ifndef sampling_hpp
#define sampling_hpp

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

#endif
