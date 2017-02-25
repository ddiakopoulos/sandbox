#include "index.hpp"
#include "vr_hmd.hpp"
#include "renderer.hpp"
#include "static_mesh.hpp"
#include "bullet_debug.hpp"

using namespace avl;

float4x4 compute_model_matrix(const StaticMesh & m)
{
	return mul(m.get_pose().matrix(), make_scaling_matrix(m.get_scale()));
}

float4x4 make_directional_light_view_proj(const uniforms::directional_light & light, const float3 & eyePoint)
{
	const Pose p = look_at_pose(eyePoint, eyePoint + -light.direction);
	const float halfSize = light.size * 0.5f;
	return mul(make_orthographic_matrix(-halfSize, halfSize, -halfSize, halfSize, -halfSize, halfSize), make_view_matrix_from_pose(p));
}

float4x4 make_spot_light_view_proj(const uniforms::spot_light & light)
{
	const Pose p = look_at_pose(light.position, light.position + light.direction);
	return mul(make_perspective_matrix(to_radians(light.cutoff * 2.0f), 1.0f, 0.1f, 1000.f), make_view_matrix_from_pose(p));
}

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
	const OpenVR_Controller & ctrl;
	std::shared_ptr<OpenVR_Controller::ControllerRenderData> renderData;

	btCollisionShape * controllerShape{ nullptr };
	BulletObjectVR * physicsObject{ nullptr };

	MotionControllerVR(BulletEngineVR & engine, const OpenVR_Controller & ctrl, std::shared_ptr<OpenVR_Controller::ControllerRenderData> renderData)
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

struct VirtualRealityApp : public GLFWApp
{
	std::unique_ptr<OpenVR_HMD> hmd;

	GlCamera firstPersonCamera;
	ShaderMonitor shaderMonitor = { "../assets/" };

	std::shared_ptr<GlShader> texturedShader;
	std::shared_ptr<GlShader> normalShader;
	std::vector<StaticMesh> sceneModels;

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
			StaticMesh cube;
			cube.set_static_mesh(make_cube(), 0.25f);
			cube.set_pose(Pose(float4(0, 0, 0, 1), float3(0, 0, 0)));

			btCollisionShape * cubeCollisionShape = new btBoxShape(to_bt(cube.get_bounds().size() * 0.5f));
			auto cubePhysicsObj = std::make_shared<BulletObjectVR>(new btDefaultMotionState(), cubeCollisionShape, physicsEngine.get_world());
			cube.set_physics_component(cubePhysicsObj.get());

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
					if (model.get_physics_component() == obj.get())
					{
						btTransform trans;
						obj->body->getMotionState()->getWorldTransform(trans);
						model.set_pose(make_pose(trans));
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
			auto model = compute_model_matrix(m);
			normalShader->uniform("u_modelMatrix", model);
			normalShader->uniform("u_modelMatrixIT", inv(transpose(model)));
			m.draw();
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

		if (hmd)
		{
			hmd->render(0.05f, 24.0f, [this](Pose eye, float4x4 projMat) { render_func(eye, projMat); });
			hmd->update();
		}

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
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