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

	std::unique_ptr<btBroadphaseInterface> broadphase = { nullptr };
	std::unique_ptr<btDefaultCollisionConfiguration> collisionConfiguration = { nullptr };
	std::unique_ptr<btCollisionDispatcher> dispatcher = { nullptr };
	std::unique_ptr<btSequentialImpulseConstraintSolver> solver = { nullptr };
	std::unique_ptr<btDiscreteDynamicsWorld> dynamicsWorld = { nullptr };

	std::vector<std::pair<OnTickCallback, int>> bulletTicks;

	static void tick_callback(btDynamicsWorld * world, btScalar time)
	{
		BulletEngineVR * engineContext = static_cast<BulletEngineVR *>(world->getWorldUserInfo());

		for (int i = 0; i < engineContext->bulletTicks.size(); ++i)
		{
			if (engineContext->bulletTicks[i].second < 0)
			{
				engineContext->bulletTicks[i].first(static_cast<float>(time), engineContext);
			}
		}
	}

public:

	BulletEngineVR()
	{
		broadphase.reset(new btDbvtBroadphase());
		collisionConfiguration.reset(new btDefaultCollisionConfiguration());
		dispatcher.reset(new btCollisionDispatcher(collisionConfiguration.get()));
		solver.reset(new btSequentialImpulseConstraintSolver());
		dynamicsWorld.reset(new btDiscreteDynamicsWorld(dispatcher, broadphase, solver, collisionConfiguration));
		dynamicsWorld->setGravity(btVector3(0, 0, -9.87));
		dynamicsWorld->setInternalTickCallback(tick_callback, static_cast<void *>(this), true);
	}

	~BulletEngineVR()
	{

	}
};


#endif
