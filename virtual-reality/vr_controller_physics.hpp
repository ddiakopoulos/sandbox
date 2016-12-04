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

struct BulletContactPointVR
{
	float3 location;
	float3 normal;
	float depth = { 1.f };
	btCollisionObject * object;
};

class BulletObjectVR
{
	btRigidBody * body = nullptr;
	btDiscreteDynamicsWorld * world = nullptr;
	btMotionState * state = nullptr;

public:

};

class BulletEngineVR
{
	using OnTickCallback = std::function<void(float, BulletEngineVR *)>;

	std::unique_ptr<btBroadphaseInterface> broadphase = { nullptr };
	std::unique_ptr<btDefaultCollisionConfiguration> collisionConfiguration = { nullptr };
	std::unique_ptr<btCollisionDispatcher> dispatcher = { nullptr };
	std::unique_ptr<btSequentialImpulseConstraintSolver> solver = { nullptr };
	std::unique_ptr<btDiscreteDynamicsWorld> dynamicsWorld = { nullptr };

	std::vector<OnTickCallback> bulletTicks;

	static void tick_callback(btDynamicsWorld * world, btScalar time)
	{
		BulletEngineVR * engineContext = static_cast<BulletEngineVR *>(world->getWorldUserInfo());

		for (auto & t : engineContext->bulletTicks)
		{
			t(static_cast<float>(time), engineContext);
		}
	}

public:

	BulletEngineVR()
	{
		broadphase.reset(new btDbvtBroadphase());
		collisionConfiguration.reset(new btDefaultCollisionConfiguration());
		dispatcher.reset(new btCollisionDispatcher(collisionConfiguration.get()));
		solver.reset(new btSequentialImpulseConstraintSolver());
		dynamicsWorld.reset(new btDiscreteDynamicsWorld(dispatcher.get(), broadphase.get(), solver.get(), collisionConfiguration.get()));
		dynamicsWorld->setGravity(btVector3(0.f, -9.87f, 0.0f));
		dynamicsWorld->setInternalTickCallback(tick_callback, static_cast<void *>(this), true);
	}

	~BulletEngineVR()
	{

	}

	void add_task(const OnTickCallback & f)
	{
		bulletTicks.push_back({ f });
	}

};

#endif
