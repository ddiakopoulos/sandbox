#include "index.hpp"
#include "vr_hmd.hpp"
#include "bullet_engine.hpp"
#include "bullet_object.hpp"
#include "bullet_debug.hpp"

using namespace avl;

#if defined(ANVIL_PLATFORM_WINDOWS)
	#define ALIGNED(n) __declspec(align(n))
#else
	#define ALIGNED(n) alignas(n)
#endif

struct StaticMeshComponent
{
	Pose pose;
	float3 scale{ 1, 1, 1 };

	GlMesh mesh;
	Geometry geom;
	Bounds3D bounds;

	BulletObjectVR * physicsComponent { nullptr };

	StaticMeshComponent() {}

	const float4x4 GetModelMatrix() const { return mul(pose.matrix(), make_scaling_matrix(scale)); }

	void SetPose(const Pose & p) { pose = p; }

	void SetRenderMode(GLenum renderMode)
	{
		if (renderMode != GL_TRIANGLE_STRIP) mesh.set_non_indexed(renderMode);
	}

	void SetStaticMesh(const Geometry & g, const float scale = 1.f)
	{
		geom = g;
		if (scale != 1.f ) rescale_geometry(geom, scale);
		bounds = geom.compute_bounds();
		mesh = make_mesh_from_geometry(geom);
	}

	void SetPhysicsComponent(BulletObjectVR * obj)
	{
		physicsComponent = obj;
	}

	void Draw() const
	{
		mesh.draw_elements();
	}

	void Update(const float dt) { }

	RaycastResult Raycast(const Ray & worldRay) const
	{
		auto localRay = pose.inverse() * worldRay;
		localRay.origin /= scale;
		localRay.direction /= scale;
		float outT = 0.0f;
		float3 outNormal = { 0, 0, 0 };
		bool hit = intersect_ray_mesh(localRay, geom, &outT, &outNormal);
		return{ hit, outT, outNormal };
	}
};

class MotionControllerVR
{
	Pose latestPose;

	void update_physics(const float dt, BulletEngineVR * engine)
	{
		physicsObject->body->clearForces();
		physicsObject->body->setWorldTransform(to_bt(latestPose.matrix()));
	}

public:

	BulletEngineVR & engine;
	const Controller & ctrl;
	std::shared_ptr<Controller::ControllerRenderData> renderData;

	btCollisionShape * controllerShape{ nullptr };
	BulletObjectVR * physicsObject{ nullptr };

	MotionControllerVR(BulletEngineVR & engine, const Controller & ctrl, std::shared_ptr<Controller::ControllerRenderData> renderData)
		: engine(engine), ctrl(ctrl), renderData(renderData)
	{

		engine.add_task([=](float time, BulletEngineVR * engine)
		{
			this->update_physics(time, engine);
		});

		controllerShape = new btBoxShape(btVector3(0.096, 0.096, 0.0123)); // fixme to use renderData

		physicsObject = new BulletObjectVR(new btDefaultMotionState(), controllerShape, engine.get_world(), 0.5f);

		physicsObject->body->setFriction(2.f);
		physicsObject->body->setRestitution(0.75);
		physicsObject->body->setGravity(btVector3(0, 0, 0));
		physicsObject->body->setActivationState(DISABLE_DEACTIVATION);

		engine.add_object(physicsObject);
	}

	void update_controller_pose(const Pose & p)
	{
		latestPose = p;
	}

};

namespace uniforms
{
	struct per_scene
	{
		static const int      binding = 0;
		float				  time;
		ALIGNED(16) float3	  ambientLight;
	};

	struct per_view
	{
		static const int      binding = 1;
		ALIGNED(16) float4x4  view;
		ALIGNED(16) float4x4  viewProj;
		ALIGNED(16) float3    eyePos;
	};
}

struct VirtualRealityApp : public GLFWApp
{
	std::unique_ptr<OpenVR_HMD> hmd;

	GlCamera firstPersonCamera;
	ShaderMonitor shaderMonitor = { "../assets/" };

	std::shared_ptr<GlShader> texturedShader;
	std::shared_ptr<GlShader> normalShader;
	std::vector<StaticMeshComponent> sceneModels;

	GlBuffer perScene;
	GlBuffer perView;

	RenderableGrid grid;

	BulletEngineVR physicsEngine;
	std::unique_ptr<MotionControllerVR> leftController;
	std::unique_ptr<PhysicsDebugRenderer> physicsDebugRenderer;
	std::vector<std::shared_ptr<BulletObjectVR>> scenePhysicsObjects;

	VirtualRealityApp() : GLFWApp(1280, 800, "VR") 
	{
		try
		{
			hmd.reset(new OpenVR_HMD());

			physicsDebugRenderer.reset(new PhysicsDebugRenderer()); // Sets up a few gl objects
			physicsDebugRenderer->setDebugMode(
				btIDebugDraw::DBG_DrawWireframe |
				btIDebugDraw::DBG_DrawContactPoints |
				btIDebugDraw::DBG_DrawConstraints |
				btIDebugDraw::DBG_DrawConstraintLimits);

			leftController.reset(new MotionControllerVR(physicsEngine, hmd->get_controller(vr::TrackedControllerRole_LeftHand), hmd->get_controller_render_data()));

			btCollisionShape * ground = new btStaticPlaneShape({ 0, 1, 0 }, 0);
			auto groundObject = std::make_shared<BulletObjectVR>(new btDefaultMotionState(), ground, physicsEngine.get_world());
			physicsEngine.add_object(groundObject.get());
			scenePhysicsObjects.push_back(groundObject);

			// Hook up debug renderer
			physicsEngine.get_world()->setDebugDrawer(physicsDebugRenderer.get());

			glfwSwapInterval(0);
		}
		catch (const std::exception & e)
		{
			std::cout << "OpenVR Exception: " << e.what() << std::endl;
		}

		texturedShader = shaderMonitor.watch("../assets/shaders/textured_model_vert.glsl", "../assets/shaders/textured_model_frag.glsl");
		normalShader = shaderMonitor.watch("../assets/shaders/normal_debug_vert.glsl", "../assets/shaders/normal_debug_frag.glsl");

		{
			StaticMeshComponent cube;
			cube.SetStaticMesh(make_cube(), 0.25f);
			cube.SetPose(Pose(float4(0, 0, 0, 1), float3(0, 0, 0)));

			btCollisionShape * cubeCollisionShape = new btBoxShape(to_bt(cube.bounds.size() * 0.5f));
			auto cubePhysicsObj = std::make_shared<BulletObjectVR>(new btDefaultMotionState(), cubeCollisionShape, physicsEngine.get_world());
			cube.SetPhysicsComponent(cubePhysicsObj.get());

			physicsEngine.add_object(cubePhysicsObj.get());
			sceneModels.push_back(std::move(cube));
			scenePhysicsObjects.push_back(cubePhysicsObj);
		}

		grid = RenderableGrid(0.25f, 24, 24);

		gl_check_error(__FILE__, __LINE__);
	}

	~VirtualRealityApp() 
	{
		hmd.reset();
	}

	void on_window_resize(int2 size) override
	{

	}

	void on_input(const InputEvent & event) override
	{

	}

	void on_update(const UpdateEvent & e) override
	{
		shaderMonitor.handle_recompile();
		if (hmd)
		{
			leftController->update_controller_pose(hmd->get_controller(vr::TrackedControllerRole_LeftHand).p);
			physicsEngine.update();

			btTransform trans;
			leftController->physicsObject->body->getMotionState()->getWorldTransform(trans);

			// Workaround until a nicer system is in place
			for (auto & obj : scenePhysicsObjects)
			{
				for (auto & model : sceneModels)
				{
					if (model.physicsComponent == obj.get())
					{
						btTransform trans;
						obj->body->getMotionState()->getWorldTransform(trans);

					}
				}
			}
		}
	}

	void render_func(Pose eye, float4x4 projMat)
	{
		//uniforms::per_view v = {};
		//v.view = eye.inverse().matrix();
		//v.viewProj = mul(projMat, eye.inverse().matrix());
		//v.eyePos = eye.position;
		//perView.set_buffer_data(sizeof(v), &v, GL_STREAM_DRAW);

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glClearColor(0.75f, 0.75f, 0.75f, 1.0f);

		auto renderModel = hmd->get_controller_render_data();

		texturedShader->bind();

		texturedShader->uniform("u_viewProj", mul(projMat, eye.inverse().matrix()));
		texturedShader->uniform("u_eye", eye.position);

		texturedShader->uniform("u_ambientLight", float3(1.0f, 1.0f, 1.0f));

		texturedShader->uniform("u_rimLight.enable", 1);

		texturedShader->uniform("u_material.diffuseIntensity", float3(1.0f, 1.0f, 1.0f));
		texturedShader->uniform("u_material.ambientIntensity", float3(1.0f, 1.0f, 1.0f));
		texturedShader->uniform("u_material.specularIntensity", float3(1.0f, 1.0f, 1.0f));
		texturedShader->uniform("u_material.specularPower", 128.0f);

		texturedShader->uniform("u_pointLights[0].position", float3(6, 10, -6));
		texturedShader->uniform("u_pointLights[0].diffuseColor", float3(1.f, 0.0f, 0.0f));
		texturedShader->uniform("u_pointLights[0].specularColor", float3(1.f, 1.0f, 1.0f));

		texturedShader->uniform("u_pointLights[1].position", float3(-6, 10, 6));
		texturedShader->uniform("u_pointLights[1].diffuseColor", float3(0.0f, 0.0f, 1.f));
		texturedShader->uniform("u_pointLights[1].specularColor", float3(1.0f, 1.0f, 1.f));

		texturedShader->uniform("u_enableDiffuseTex", 1);
		texturedShader->uniform("u_enableNormalTex", 0);
		texturedShader->uniform("u_enableSpecularTex", 0);
		texturedShader->uniform("u_enableEmissiveTex", 0);
		texturedShader->uniform("u_enableGlossTex", 0);

		texturedShader->texture("u_diffuseTex", 0, renderModel->tex, GL_TEXTURE_2D);

		for (auto pose : { hmd->get_controller(vr::TrackedControllerRole_LeftHand).p, hmd->get_controller(vr::TrackedControllerRole_RightHand).p })
		{
			texturedShader->uniform("u_modelMatrix", pose.matrix());
			texturedShader->uniform("u_modelMatrixIT", inverse(transpose(pose.matrix())));
			renderModel->mesh.draw_elements();
		}

		texturedShader->unbind();

		normalShader->bind();
		normalShader->uniform("u_viewProj", mul(projMat, eye.inverse().matrix()));
		for (const auto & m : sceneModels)
		{
			auto model = m.GetModelMatrix();
			normalShader->uniform("u_modelMatrix", model);
			normalShader->uniform("u_modelMatrixIT", inv(transpose(model)));
			m.Draw();
		}
		normalShader->unbind();

		grid.render(projMat, eye.inverse().matrix(), float3(0, -.01f, 0));

		physicsEngine.get_world()->debugDrawWorld();
		physicsDebugRenderer->draw(projMat, eye.inverse().matrix());
	}

	void on_draw() override
	{
		glfwMakeContextCurrent(window);

		int width, height;
		glfwGetWindowSize(window, &width, &height);
		glViewport(0, 0, width, height);

		glEnable(GL_CULL_FACE);
		glEnable(GL_DEPTH_TEST);

		// FPS Camera Only
		const float4x4 projMatrix = firstPersonCamera.get_projection_matrix((float)width / (float)height);
		const float4x4 viewMatrix = firstPersonCamera.get_view_matrix();
		const float4x4 viewProjMatrix = mul(projMatrix, viewMatrix);

		uniforms::per_scene b = {};
		b.time = 0.0f;
		b.ambientLight = float3(1.0f);
		perScene.set_buffer_data(sizeof(b), &b, GL_STREAM_DRAW);

		glBindBufferBase(GL_UNIFORM_BUFFER, uniforms::per_scene::binding, perScene);
		glBindBufferBase(GL_UNIFORM_BUFFER, uniforms::per_view::binding, perView);

		if (hmd)
		{
			hmd->render(0.05f, 24.0f, [this](Pose eye, float4x4 projMat) { render_func(eye, projMat); });
			hmd->update();
		}

		//glBindFramebuffer(GL_FRAMEBUFFER, 0);

		glfwSwapBuffers(window);
		gl_check_error(__FILE__, __LINE__);
	}

};

int main(int argc, char * argv[])
{
	VirtualRealityApp app;
	app.main_loop();
	return EXIT_SUCCESS;
}