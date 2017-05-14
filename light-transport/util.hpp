#ifndef light_transport_utils_hpp
#define light_transport_utils_hpp

#pragma once

#include "geometric.hpp"
#include "util.hpp"

//////////////
//   Math   //
//////////////

inline float luminance(float3 c) 
{ 
	return 0.2126f * c.x + 0.7152f * c.y + 0.0722f * c.z; 
}

// Make a tangent space coordinate system for isotropic BRDFs. vec1 (the normal) must be normalized beforehand.
inline void make_tangent_frame(const float3 & normal, float3 & tangent, float3 & bitangent)
{
	if (std::fabs(normal.x) > std::fabs(normal.y)) tangent = normalize(float3(normal.z, 0, -normal.x));
	else tangent = normalize(float3(0, -normal.z, normal.y));
	bitangent = normalize(cross(normal, tangent));
	//if (std::abs(normal.x) > std::abs(normal.y)) tangent = float3(0.0f, 1.0f, 0.0f);
	//else tangent = float3(1.0f, 0.0f, 0.0f);
	//bitangent = normalize(cross(normal, tangent));
	//tangent = cross(bitangent, normal);
}

// Add an epsilon to a ray origin to avoid self intersections
inline float3 add_epsilon(const float3 & point, const float3 & direction)
{
	return point;// point + (direction * 0.0001f);
}

inline float dielectric_reflectance(const float eta, const float cosThetaI, float & cosThetaT)
{
	/*
	if (cosThetaI < 0.0f) 
	{
		eta = 1.0f / eta;
		cosThetaI = -cosThetaI;
	}
	*/
	float sinThetaTSq = eta * eta * (1.0f - cosThetaI * cosThetaI);
	if (sinThetaTSq > 1.0f) 
	{
		cosThetaT = 0.0f;
		return 1.0f;
	}
	cosThetaT = std::sqrt(std::max(1.0f - sinThetaTSq, 0.0f));

	float Rs = (eta * cosThetaI - cosThetaT) / (eta * cosThetaI + cosThetaT);
	float Rp = (eta * cosThetaT - cosThetaI) / (eta * cosThetaT + cosThetaI);

	return (Rs*Rs + Rp*Rp)*0.5f;
}

const float DiracAcceptanceThreshold = 1e-3f;

inline bool reflection_constraint(const float3 & wi, const float3 & wo)
{
	return std::abs(wi.z*wo.z - wi.x*wo.x - wi.y*wo.y - 1.0f) < DiracAcceptanceThreshold;
}

static inline bool refraction_constraint(const float3 &wi, const float3 &wo, float eta, float cosThetaT)
{
	float dotP = -wi.x*wo.x*eta - wi.y*wo.y*eta - std::copysign(cosThetaT, wi.z)*wo.z;
	return std::abs(dotP - 1.0f) < DiracAcceptanceThreshold;
}


#endif
