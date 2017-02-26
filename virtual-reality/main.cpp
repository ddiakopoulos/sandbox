#include "index.hpp"
#include "vr_hmd.hpp"
#include "renderer.hpp"
#include "static_mesh.hpp"
#include "bullet_debug.hpp"

using namespace avl;

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

struct viewport
{
	float2 bmin, bmax;
	GLuint texture;
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

struct Scene
{
	RenderableGrid grid {0.25f, 24, 24 };

	std::vector<std::shared_ptr<BulletObjectVR>> physicsObjects;

	std::vector<StaticMesh> models;
	std::vector<StaticMesh> controllers;

	std::shared_ptr<Material> debugMaterial;
	std::shared_ptr<Material> texturedMaterial;

	std::vector<Renderable *> gather()
	{
		std::vector<Renderable *> objectList;
		for (auto & model : models) objectList.push_back(&model);
		for (auto & ctrlr : controllers) objectList.push_back(&ctrlr);
		return objectList;
	}
};

struct VirtualRealityApp : public GLFWApp
{
	std::unique_ptr<Renderer> renderer;
	std::unique_ptr<OpenVR_HMD> hmd;

	GlCamera debugCam;
	ShaderMonitor shaderMonitor = { "../assets/" };
	
	std::vector<viewport> viewports;
	Scene scene;

	BulletEngineVR physicsEngine;
	std::unique_ptr<MotionControllerVR> leftController;
	std::unique_ptr<PhysicsDebugRenderer> physicsDebugRenderer;

	VirtualRealityApp() : GLFWApp(1280, 800, "VR") 
	{
		int windowWidth, windowHeight;
		glfwGetWindowSize(window, &windowWidth, &windowHeight);

		scene.grid.set_origin(float3(0, -.01f, 0));

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
			scene.physicsObjects.push_back(groundObject);

			// Hook up debug renderer
			physicsEngine.get_world()->setDebugDrawer(physicsDebugRenderer.get());

			const uint2 targetSize = hmd->get_recommended_render_target_size();
			renderer.reset(new Renderer({ (float)targetSize.x, (float)targetSize.y }));

			glfwSwapInterval(0);
		}
		catch (const std::exception & e)
		{
			std::cout << "OpenVR Exception: " << e.what() << std::endl;
			renderer.reset(new Renderer({ (float)windowWidth, (float)windowHeight }));
		}

		auto normalShader = shaderMonitor.watch("../assets/shaders/normal_debug_vert.glsl", "../assets/shaders/normal_debug_frag.glsl");
		scene.debugMaterial = std::make_shared<DebugMaterial>(normalShader);

		if (hmd)
		{
			// This section sucks I think:
			auto controllerRenderModel = hmd->get_controller_render_data();
			auto texturedShader = shaderMonitor.watch("../assets/shaders/textured_model_vert.glsl", "../assets/shaders/textured_model_frag.glsl");
			auto texturedMaterial = std::make_shared<TexturedMaterial>(texturedShader);
			texturedMaterial->set_diffuse_texture(controllerRenderModel->tex);
			scene.texturedMaterial = texturedMaterial;

			// Create renderable controllers
			for (int i = 0; i < 2; ++i)
			{
				StaticMesh controller;
				controller.set_static_mesh(controllerRenderModel->mesh, 1.0f);
				controller.set_pose(Pose(float4(0, 0, 0, 1), float3(0, 0, 0)));
				controller.set_material(scene.texturedMaterial.get());
				scene.controllers.push_back(std::move(controller));
			}

		}

		{
			StaticMesh cube;
			cube.set_static_mesh(make_cube(), 0.25f);
			cube.set_pose(Pose(float4(0, 0, 0, 1), float3(0, 0, 0)));
			cube.set_material(scene.debugMaterial.get());

			btCollisionShape * cubeCollisionShape = new btBoxShape(to_bt(cube.get_bounds().size() * 0.5f));
			auto cubePhysicsObj = std::make_shared<BulletObjectVR>(new btDefaultMotionState(), cubeCollisionShape, physicsEngine.get_world());
			cube.set_physics_component(cubePhysicsObj.get());

			physicsEngine.add_object(cubePhysicsObj.get());
			scene.physicsObjects.push_back(cubePhysicsObj);
			scene.models.push_back(std::move(cube));
		}

		//grid = RenderableGrid

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
			for (auto & obj : scene.physicsObjects)
			{
				for (auto & model : scene.models)
				{
					if (model.get_physics_component() == obj.get())
					{
						btTransform trans;
						obj->body->getMotionState()->getWorldTransform(trans);
						model.set_pose(make_pose(trans));
					}
				}
			}

			// Update the the pose of the controller mesh we render
			scene.controllers[0].set_pose(hmd->get_controller(vr::TrackedControllerRole_LeftHand).p);
			scene.controllers[1].set_pose(hmd->get_controller(vr::TrackedControllerRole_RightHand).p);
		}

		// Iterate scene and make objects visible to the renderer
		auto renderableObjectsInScene = scene.gather();
		for (auto & obj : renderableObjectsInScene) { renderer->add_renderable(obj); }
		renderer->add_debug_renderable(&scene.grid);
	}

	void on_draw() override
	{
		glfwMakeContextCurrent(window);

		int width, height;
		glfwGetWindowSize(window, &width, &height);
		glViewport(0, 0, width, height);

		//grid.render(projMat, eye.inverse().matrix(), );

		physicsEngine.get_world()->debugDrawWorld();
		//physicsDebugRenderer->draw(projMat, eye.inverse().matrix());

		if (hmd)
		{
			EyeData left = { hmd->get_eye_pose(vr::Hmd_Eye::Eye_Left), hmd->get_proj_matrix(vr::Hmd_Eye::Eye_Left, 0.01, 25.f) };
			EyeData right = { hmd->get_eye_pose(vr::Hmd_Eye::Eye_Right), hmd->get_proj_matrix(vr::Hmd_Eye::Eye_Right, 0.01, 25.f) };
			renderer->set_eye_data(left, right);

			renderer->render_frame();
			hmd->submit(renderer->get_eye_texture(Eye::LeftEye), renderer->get_eye_texture(Eye::LeftEye));
			hmd->update();
		}
		else
		{
			Bounds2D rect{ { 0.f ,0.f },{ (float)width,(float)height } };

			viewports.clear();

			glUseProgram(0);
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			glClear(GL_COLOR_BUFFER_BIT);
			glColor4f(1, 1, 1, 1);

			const float4x4 projMatrix = debugCam.get_projection_matrix(float(width) / float(height));
			const float4x4 viewMatrix = debugCam.get_view_matrix();
			const float4x4 viewProjMatrix = mul(projMatrix, viewMatrix);

			EyeData centerEye = { debugCam.get_pose(), projMatrix };
			renderer->set_eye_data(centerEye, centerEye);

			renderer->render_frame();

			int mid = (rect.min().x + rect.max().x) / 2.f;
			viewport leftviewport = { rect.min(), { mid - 2.f, rect.max().y }, renderer->get_eye_texture(Eye::LeftEye).id() };
			viewport rightViewport ={ { mid + 2.f, rect.min().y }, rect.max(), renderer->get_eye_texture(Eye::RightEye).id() };
	
			viewports.push_back(leftviewport);
			viewports.push_back(rightViewport);

			for (auto & v : viewports)
			{
				glViewport(v.bmin.x, height - v.bmax.y, v.bmax.x - v.bmin.x, v.bmax.y - v.bmin.y);
				glActiveTexture(GL_TEXTURE0);
				glEnable(GL_TEXTURE_2D);
				glBindTexture(GL_TEXTURE_2D, v.texture);
				glBegin(GL_QUADS);
				glTexCoord2f(0, 0); glVertex2f(-1, -1);
				glTexCoord2f(1, 0); glVertex2f(+1, -1);
				glTexCoord2f(1, 1); glVertex2f(+1, +1);
				glTexCoord2f(0, 1); glVertex2f(-1, +1);
				glEnd();
				glDisable(GL_TEXTURE_2D);
			}

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