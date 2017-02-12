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

struct BulletContactPointVR
{
	float depth{ 1.f };
	float3 location;
	float3 normal;
	btCollisionObject * object;
};

class BulletObjectVR
{
	btDiscreteDynamicsWorld * world = { nullptr };

public:

	btMotionState * state = { nullptr };
	std::unique_ptr<btRigidBody> body = { nullptr };

	BulletObjectVR(btMotionState * state, btCollisionShape * collisionShape, btDiscreteDynamicsWorld * world, float mass = 0.0f) : state(state), world(world)
	{
		btVector3 inertia(0, 0, 0);
		if (mass > 0.0f) collisionShape->calculateLocalInertia(mass, inertia);
		btRigidBody::btRigidBodyConstructionInfo constructionInfo(mass, state, collisionShape, inertia);
		body.reset(new btRigidBody(constructionInfo));
	}

	BulletObjectVR(float4x4 xform, btCollisionShape * collisionShape, btDiscreteDynamicsWorld * world, float mass = 0.0f) : world(world)
	{

	}
	
	std::vector<BulletContactPointVR> CollideWorld() const
	{
		struct CollideCallback : public btCollisionWorld::ContactResultCallback
		{
			btRigidBody & body;
			std::vector<BulletContactPointVR> contacts;

			CollideCallback(btRigidBody & target) : btCollisionWorld::ContactResultCallback(), body(target) { }

			bool needsCollision(btBroadphaseProxy * proxy) const
			{
				if (!btCollisionWorld::ContactResultCallback::needsCollision(proxy)) return false; // superclass pre-filters
				return body.checkCollideWithOverride(static_cast<btCollisionObject*>(proxy->m_clientObject)); // avoid contraint contacts
			}

			// Called for each contact point
			btScalar addSingleResult(btManifoldPoint & cp, const btCollisionObjectWrapper * colObj0, int, int, const btCollisionObjectWrapper * colObj1, int, int)
			{
				btVector3 point, n;
				BulletContactPointVR p;

				if (colObj0->m_collisionObject == &body)
				{
					point = cp.m_localPointA;
					n = cp.m_normalWorldOnB;
					p.object = const_cast<btCollisionObject*>(colObj1->m_collisionObject);
				}
				else
				{
					point = cp.m_localPointB;
					n = -cp.m_normalWorldOnB;
					p.object = const_cast<btCollisionObject*>(colObj0->m_collisionObject);
				}

				p.location = { point.x(), point.y(), point.z() };
				p.normal = { n.x(), n.y(), n.z() };
				p.depth = fabs(cp.getDistance());

				contacts.push_back(p);

				return 0;
			}
		};

		CollideCallback callback(*body.get());
		world->contactTest(body.get(), callback);

		return callback.contacts;
	}

	std::vector<BulletContactPointVR> CollideWith(btCollisionObject * other) const
	{

	}

	bool FirstContact(const float4x4 & src, BulletContactPointVR & contact) const
	{

	}

	~BulletObjectVR()
	{
		world->removeCollisionObject(body.get());
	}

};


#endif
