#include "index.hpp"
#include "vr_hmd.hpp"

using namespace avl;

std::shared_ptr<GlShader> make_watched_shader(ShaderMonitor & mon, const std::string vertexPath, const std::string fragPath, const std::string geomPath = "")
{
	std::shared_ptr<GlShader> shader = std::make_shared<GlShader>(read_file_text(vertexPath), read_file_text(fragPath), read_file_text(geomPath));
	mon.add_shader(shader, vertexPath, fragPath);
	return shader;
}

struct VirtualRealityApp : public GLFWApp
{
	std::unique_ptr<OpenVR_HMD> hmd;
	GlCamera firstPersonCamera;
	ShaderMonitor shaderMonitor;
	std::shared_ptr<GlShader> texturedShader;
	std::shared_ptr<GlShader> normalShader;
	std::vector<Renderable> debugModels;

	VirtualRealityApp() : GLFWApp(1280, 720, "VR") 
	{
		try
		{
			hmd.reset(new OpenVR_HMD());
		}
		catch (const std::exception & e)
		{
			std::cout << "OpenVR Exception: " << e.what() << std::endl;
		}

		texturedShader = make_watched_shader(shaderMonitor, "../assets/shaders/textured_model_vert.glsl", "../assets/shaders/textured_model_frag.glsl");
		normalShader = make_watched_shader(shaderMonitor, "../assets/shaders/normal_debug_vert.glsl", "../assets/shaders/normal_debug_frag.glsl");

		Renderable cube = Renderable(make_cube());
		cube.pose = Pose(make_rotation_quat_axis_angle({ 1, 0, 1 }, ANVIL_PI / 4), float3(-5, 0, 0));
		debugModels.push_back(std::move(cube));

		gl_check_error(__FILE__, __LINE__);
	}

	~VirtualRealityApp() {}

	void on_window_resize(int2 size) override
	{

	}

	void on_input(const InputEvent & event) override
	{

	}

	void on_update(const UpdateEvent & e) override
	{
		shaderMonitor.handle_recompile();
		if (hmd) hmd->update();
	}

	void render_func(Pose eye, float4x4 projMat)
	{
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

		auto renderModel = hmd->get_controller_render_data();

		texturedShader->bind();

		texturedShader->uniform("u_viewProj", mul(eye.inverse().matrix(), projMat));
		texturedShader->uniform("u_eye", eye.position);

		texturedShader->uniform("u_ambientLight", float3(1.0f, 1.0f, 1.0f));

		texturedShader->uniform("u_rimLight.enable", 0);

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
		normalShader->uniform("u_viewProj", mul(eye.inverse().matrix(), projMat));
		for (const auto & model : debugModels)
		{
			normalShader->uniform("u_modelMatrix", model.get_model());
			normalShader->uniform("u_modelMatrixIT", inv(transpose(model.get_model())));
			model.draw();
		}
		normalShader->unbind();

	}

	void on_draw() override
	{
		glfwMakeContextCurrent(window);

		int width, height;
		glfwGetWindowSize(window, &width, &height);
		glViewport(0, 0, width, height);

		//const float4x4 projMatrix = firstPersonCamera.get_projection_matrix((float)width / (float)height);
		//const float4x4 viewMatrix = firstPersonCamera.get_view_matrix();
		//const float4x4 viewProjMatrix = mul(projMatrix, viewMatrix);

		if (hmd) hmd->render(0.05f, 24.0f, [this](Pose eye, float4x4 projMat) { render_func(eye, projMat); });

		//gl_check_error(__FILE__, __LINE__);

		glfwSwapBuffers(window);
	}

};

int main(int argc, char * argv[])
{
	VirtualRealityApp app;
	app.main_loop();
	return EXIT_SUCCESS;
}