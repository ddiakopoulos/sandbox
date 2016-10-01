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
		uint32_t axis = 0; // split axis
		float position = 0.f; // split position
		std::vector<std::shared_ptr<Traceable>> data;

		Node(const uint32_t axis = 0, const float position = 0.f) : axis(axis), position(position) {}
		~Node() { if (left) delete left; if (right) delete right; }

		bool is_leaf() const { return (left == nullptr && right == nullptr); }
	};

	std::vector<std::shared_ptr<Traceable>> objects;
	bool initialized = false;

	float leafThreshold = 1.f;
	Node * root;

public:

	BVH(std::vector<std::shared_ptr<Traceable>> objects) : objects(objects) {}
	~BVH() { if (root) delete root; }

	void build()
	{
		root = new Node();
		build_recursive(root, 0, objects);
		initialized = true;
	}

	void build_recursive(Node * node, uint32_t depth, std::vector<std::shared_ptr<Traceable>> & objects)
	{
		Bounds3D nodeBounds = objects[0]->world_bounds();
		for (size_t i = 0; i < objects.size(); i++)
		{
			nodeBounds = nodeBounds.add(objects[i]->world_bounds());
		}
		node->bounds = nodeBounds;

		// Recursion limit was reached, create a new leaf
		if (objects.size() <= leafThreshold)
		{
			node->data.insert(node->data.end(), objects.begin(), objects.end());
			return;
		}

		// Split the node based on its longest axis
		node->axis = nodeBounds.maximum_extent();

		const uint32_t median = objects.size() >> 1;
		std::nth_element(objects.begin(), objects.begin() + median, objects.end(), ObjectComparator(node->axis));
		node->position = objects[median]->world_bounds().center()[node->axis];

		node->left = new Node();
		node->right = new Node();

		std::vector<std::shared_ptr<Traceable>> leftList(median);
		std::vector<std::shared_ptr<Traceable>> rightList(objects.size() - median);
		std::copy(objects.begin(), objects.begin() + median, leftList.begin());
		std::copy(objects.begin() + median, objects.end(), rightList.begin());

		build_recursive(node->left, depth + 1, leftList);
		build_recursive(node->right, depth + 1, rightList);
	}

	// Compute entire bounds of BVH in world space
	Bounds3D world_bounds() const
	{

	}

	RayIntersection intersect(const Ray & ray)
	{

	}

	bool intersect_p(const Ray & ray)
	{

	}
};

#endif
