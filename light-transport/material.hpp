#ifndef material_hpp
#define material_hpp

#pragma once

#include "geometric.hpp"
#include "util.hpp"

struct Material
{
	float3 diffuse = { 0, 0, 0 };
	float3 emissive = { 0, 0, 0 };

	// Math borrowed from Kevin Beason's `smallpt`
	Ray get_reflected_ray(const Ray & r, const float3 & p, const float3 & n, UniformRandomGenerator & gen) const
	{
		// ideal specular reflection
		float roughness = 0.925;
		float3 refl = r.direction - n * 2.0f * dot(n, r.direction);
		refl = normalize(float3(refl.x + (gen.random_float() - 0.5f) * roughness,
		refl.y + (gen.random_float() - 0.5f) * roughness,
		refl.z + (gen.random_float() - 0.5f) * roughness));
		return Ray(p, refl);

		// ideal diffuse reflection
		float3 NdotL = clamp(dot(n, r.direction) < 0.0f ? n : n * -1.f, 0.f, 1.f); // orient the surface normal
		float r1 = 2 * ANVIL_PI * gen.random_float(); // random angle around a hemisphere
		float r2 = gen.random_float();
		float r2s = sqrt(r2); // distance from center

		// u is perpendicular to w && v is perpendicular to u and w
		float3 u = normalize(cross((abs(NdotL.x) > 0.1f ? float3(0, 1, 0) : float3(1, 1, 1)), NdotL));
		float3 v = cross(NdotL, u);
        float3 d = normalize(u * std::cos(r1) * r2s + v * std::sin(r1) * r2s + NdotL * std::sqrt(1.f - r2)); // random reflection ray
		return Ray(p, d);
	}
};

#endif
