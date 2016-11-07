#include "index.hpp"
#include "vr_hmd.hpp"

using namespace avl;

struct VirtualRealityApp : public GLFWApp
{

	std::unique_ptr<OpenVR_HMD> hmd;
	GlCamera firstPersonCamera;

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

	}

	void on_draw() override
	{
		glfwMakeContextCurrent(window);

		int width, height;
		glfwGetWindowSize(window, &width, &height);
		glViewport(0, 0, width, height);

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glClearColor(1.0f, 0.1f, 0.0f, 1.0f);

		const float4x4 projMatrix = firstPersonCamera.get_projection_matrix((float)width / (float)height);
		const float4x4 viewMatrix = firstPersonCamera.get_view_matrix();
		const float4x4 viewProjMatrix = mul(projMatrix, viewMatrix);

		gl_check_error(__FILE__, __LINE__);
		glfwSwapBuffers(window);
	}

};

int main(int argc, char * argv[])
{
	VirtualRealityApp app;
	app.main_loop();
	return EXIT_SUCCESS;
}