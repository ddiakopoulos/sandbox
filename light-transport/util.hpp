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

inline std::pair<float3, float3> make_tangent_frame(const float3 & N)
{
	float3 tangent, bitangent;
	if (std::abs(N.x) > std::abs(N.y)) tangent = float3(0.0f, 1.0f, 0.0f);
	else tangent = float3(1.0f, 0.0f, 0.0f);
	bitangent = safe_normalize(cross(tangent, N));
	tangent = cross(tangent, bitangent);
	return{ tangent, bitangent };
}

#endif
