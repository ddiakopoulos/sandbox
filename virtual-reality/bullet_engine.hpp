#pragma once

#ifndef bullet_engine_vr_hpp
#define bullet_engine_vr_hpp

#include "btBulletCollisionCommon.h"
#include "btBulletDynamicsCommon.h"

#include "linalg_util.hpp"
#include "geometric.hpp"
#include "geometry.hpp"
#include "bullet_utils.hpp"
#include <functional>
#include <vector>

#include "bullet_object.hpp"

using namespace avl;

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

	~BulletEngineVR() { }

	void add_object(BulletObjectVR * object)
	{
		dynamicsWorld->addRigidBody(object->body.get());
		object->body->setDamping(0.3, 0.5);
	}

	void remove_object(BulletObjectVR * object)
	{
		object->get_world()->removeRigidBody(object->body.get());
	}

	void add_task(const OnTickCallback & f)
	{
		bulletTicks.push_back({ f });
	}

};

#endif // end bullet_engine_vr_hpp
