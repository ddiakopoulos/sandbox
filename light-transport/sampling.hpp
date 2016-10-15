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

// Uniformly sample a vector on the unit sphere 
inline float3 uniform_sphere(const float2 & xi)
{
	float z = 2.f * xi.x - 1.f;
	float r = std::sqrt(std::max(0.f, 1.f - z * z));
	float phi = 2.f * ANVIL_PI * xi.y;
	return float3(r * std::cos(phi), r * std::sin(phi), z);
}

inline float uniform_sphere_pdf()
{
	return 1.f / (4.f * ANVIL_PI);
}

// Sample from a uniform hemisphere around the pole (0,0,1)
inline float3 uniform_hemisphere(const float2 & xi)
{
	float phi = xi.x * ANVIL_TWO_PI;
	float r = std::sqrt(std::max(1.f - xi.y * xi.y, 0.f));
	return float3(std::cos(phi) * r, std::sin(phi) * r, xi.y);
}

// Return the PDF of recieving a sample via the `uniform_hemisphere` sampling method
inline float uniform_hemisphere_pdf(const float3 & P)
{
	return 1.f / ANVIL_TWO_PI;
}

// Sample from a cosine-weighted hemisphere around the pole (0,0,1)
inline float3 cosine_hemisphere(const float2 & xi)
{
	float phi = xi.x * ANVIL_TWO_PI;
	float r = std::sqrt(xi.y);
	return float3(std::cos(phi) * r, std::sin(phi) * r, std::sqrt(std::max(1.0f - xi.y, 0.0f)));
}

// Return the PDF of recieving a sample via the `cosine_hemisphere` sampling method
inline float cosine_hemisphere_pdf(const float3 & P)
{
	return P.z * ANVIL_INV_PI;
}

// Sample from a cosine-weighted hemisphere centered on normal N
inline float3 cosine_hemisphere(const float2 & xi, const float3 & N)
{
	float phi = xi.x * ANVIL_TWO_PI;
	float3 w = N;
	float3 u = normalize((cross((std::abs(w.x) > 0.1f ? float3(0, 1, 0) : float3(1, 1, 1)), w))); // u is perpendicular to w
	float3 v = cross(w, u); // v is perpendicular to u and w
	return normalize(u * std::cos(phi) * std::sqrt(xi.y) + v * std::sin(phi) *std::sqrt(xi.y) + w * std::sqrt(1.0f - xi.y));
}

#endif
