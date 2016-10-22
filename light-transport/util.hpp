#ifndef light_transport_utils_hpp
#define light_transport_utils_hpp

#pragma once

#include "geometric.hpp"
#include "util.hpp"

////////////////
//   Timers   //
////////////////

class PerfTimer
{
	std::chrono::high_resolution_clock::time_point t0;
	double timestamp = 0.f;
public:
	PerfTimer() {};
	const double & get() { return timestamp; }
	void start() { t0 = std::chrono::high_resolution_clock::now(); }
	void stop() { timestamp = std::chrono::duration<float>(std::chrono::high_resolution_clock::now() - t0).count() * 1000; }
};

class ScopedTimer
{
	PerfTimer t;
	std::string message;
public:
	ScopedTimer(std::string message) : message(message) { t.start(); }
	~ScopedTimer()
	{
		t.stop();
		std::cout << message << " completed in " << t.get() << " ms" << std::endl;
	}
};

//////////////
//   Math   //
//////////////

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

inline float dielectric_reflectance(float eta, float cosThetaI, float & cosThetaT)
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
	cosThetaT = std::sqrt(max(1.0f - sinThetaTSq, 0.0f));

	float Rs = (eta * cosThetaI - cosThetaT) / (eta * cosThetaI + cosThetaT);
	float Rp = (eta * cosThetaT - cosThetaI) / (eta * cosThetaT + cosThetaI);

	return (Rs*Rs + Rp*Rp)*0.5f;
}

inline float dielectric_reflectance(float eta, float cosThetaI)
{
	float cosThetaT;
	return dielectric_reflectance(eta, cosThetaI, cosThetaT);
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
