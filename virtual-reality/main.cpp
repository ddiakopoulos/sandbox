#include "index.hpp"
#include "btBulletCollisionCommon.h"
#include "vr_hmd.hpp"

using namespace avl;

btVector3 to_bt(const float3 & i_v)
{
	return btVector3(static_cast<btScalar>(i_v.x), static_cast<btScalar>(i_v.y), static_cast<btScalar>(i_v.z));
}

btQuaternion to_bt(const float4 & q)
{
	return btQuaternion(static_cast<btScalar>(q.x), static_cast<btScalar>(q.y), static_cast<btScalar>(q.z), static_cast<btScalar>(q.w));
}

btMatrix3x3 to_bt(const float3x3 & i_m)
{
	auto rQ = make_rotation_quat_from_rotation_matrix(i_m);
	return btMatrix3x3(to_bt(rQ));
}

btTransform to_bt(const float4x4 & xform)
{
	auto r = get_rotation_submatrix(xform);
	auto t = xform.z.xyz();
	return btTransform(to_bt(r), to_bt(t));
}

float3 from_bt(const btVector3 & v)
{
	return float3(v.x(), v.y(), v.z());
}

float4 from_bt(const btQuaternion & q)
{
	return float4(q.x(), q.y(), q.z(), q.w());
}

float3x3 from_bt(const btMatrix3x3 & i_m)
{
	btQuaternion q;
	i_m.getRotation(q);
	return get_rotation_submatrix(make_rotation_matrix(from_bt(q)));
}

float4x4 from_bt(const btTransform & t)
{
	auto tM = make_translation_matrix(from_bt(t.getOrigin()));
	auto rM = make_rotation_matrix(from_bt(t.getRotation()));
	return mul(tM, rM);
}

struct VirtualRealityApp : public GLFWApp
{
	VirtualRealityApp() : GLFWApp(100, 100, "VR") {}
	~VirtualRealityApp() {}
};

int main(int argc, char * argv[])
{
	VirtualRealityApp app;
	app.main_loop();
	return EXIT_SUCCESS;
}