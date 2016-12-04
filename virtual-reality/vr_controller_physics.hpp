#pragma once

#ifndef vr_controller_physics_hpp
#define vr_controller_physics_hpp

#include "btBulletCollisionCommon.h"
#include "btBulletDynamicsCommon.h"

#include "linalg_util.hpp"
#include "geometric.hpp"
#include "geometry.hpp"
#include "bullet_utils.hpp"
#include <functional>
#include <vector>

using namespace avl;

class BulletEngineVR
{
	using OnTickCallback = std::function<void(float, BulletEngineVR *)>;

	btBroadphaseInterface * broadphase = { nullptr };
	btDefaultCollisionConfiguration* collisionConfiguration = { nullptr };
	btCollisionDispatcher * dispatcher = { nullptr };
	btSequentialImpulseConstraintSolver * solver = { nullptr };
	btDiscreteDynamicsWorld * dynamicsWorld = { nullptr };

	std::vector<std::pair<OnTickCallback, int>> bulletTicks;

public:
	BulletEngineVR()
	{

	}

	~BulletEngineVR()
	{

	}
};


#endif
