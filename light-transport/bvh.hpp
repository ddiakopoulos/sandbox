#ifndef bvh_hpp
#define bvh_hpp

#pragma once

#include "geometric.hpp"
#include <vector>
#include <memory>

using namespace avl;

class BVH
{

	struct ObjectComparator
	{
		uint32_t split;
		ObjectComparator(const uint32_t axis = 0) : split(axis) { }
		bool operator() (const Traceable * a, const Traceable * b) const { return true; }
	};

	struct Node
	{
		Node * left = nullptr;
		Node * right = nullptr;
		Bounds3D bounds;
		uint32_t axis = 0;
		float position = 0.f;
		std::vector<std::shared_ptr<Traceable>> objects;

		Node(const uint32_t axis = 0, const float position = 0.f) : axis(axis), position(position) {}
		~Node() { if (left) delete left; if (right) delete right; }

		bool is_leaf() const { return (left == nullptr && right == nullptr); }
	};

	std::vector<std::shared_ptr<Traceable>> objects;
	bool initialized = false;

public:

	BVH(std::vector<std::shared_ptr<Traceable>> objects) : objects(objects) {}
	~BVH() {}

	void build()
	{
		// ... 
		initialized = true;
	}

	void build_recursive(Node * node, uint32_t depth, std::vector<std::shared_ptr<Traceable>> & objects)
	{

	}

	// Compute entire bounds of BVH in world space
	Bounds3D world_space() const
	{

	}

	RayIntersection intersect(const Ray & ray)
	{

	}

	RayIntersection intersect_p(const Ray & ray)
	{

	}
};

#endif
