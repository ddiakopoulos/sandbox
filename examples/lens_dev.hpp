#include "index.hpp"

#include <ratio>

// Diagonal to vertical fov
inline float dfov_to_vfov(float diagonalFov, float aspectRatio)
{
	return 2.f * atan(tan(diagonalFov / 2.f) / sqrt(1.f + aspectRatio * aspectRatio));
}

// Diagonal to horizontal fov
inline float dfov_to_hfov(float diagonalFov, float aspectRatio)
{
	return 2.f * atan(tan(diagonalFov / 2.f) / sqrt(1.f + 1.f / (aspectRatio * aspectRatio)));
}

// Vertical to diagonal fov
inline float vfov_to_dfov(float vFov, float aspectRatio)
{
	return 2.f * atan(tan(vFov / 2.f) * sqrt(1.f + aspectRatio * aspectRatio));
}

// Horizontal to diagonal fov
inline float hfov_to_dfov(float hFov, float aspectRatio)
{
	return 2.f * atan(tan(hFov / 2.f) * sqrt(1.f + 1.f / (aspectRatio * aspectRatio)));
}

// Horizontal to vertical fov
inline float hfov_to_vfov(float hFov, float aspectRatio)
{
	return 2.f * atan(tan(hFov / 2.f) / aspectRatio);
}

struct ExperimentalApp : public GLFWApp
{
	std::unique_ptr<gui::ImGuiManager> igm;

    uint64_t frameCount = 0;
    GlCamera camera;
    RenderableGrid grid;

    ExperimentalApp() : GLFWApp(600, 600, "Lens Dev")
    {
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);

		igm.reset(new gui::ImGuiManager(window));
		gui::make_dark_theme();

        grid = RenderableGrid(1, 100, 100);
        gl_check_error(__FILE__, __LINE__);
        camera.look_at({0, 2.5, -2.5}, {0, 2.0, 0});

		const float target_dfov = to_radians(100.f);

		const int horizontalAspect = std::ratio<1200, 1080>::num;
		const int verticalAspect = std::ratio<1080, 1200>::num;

		const float overlap_percent = 1.f;

		float diagonal_aspect = std::sqrt(pow(horizontalAspect, 2) + pow(verticalAspect, 2));
		float hfov_original = 2.f * atan(tan(target_dfov / 2.f) * (horizontalAspect / diagonal_aspect));
	 	float vfov = 2.f * atan(tan(target_dfov / 2.f) * (verticalAspect / diagonal_aspect));
		float hfov_overlap = hfov_original * (2.f - overlap_percent);
		float aspect_overlap = tan(hfov_overlap / 2.f) / tan(vfov / 2);
		float diagonal_aspect_overlap = sqrt(pow(tan(hfov_overlap / 2.f), 2) + pow(verticalAspect, 2));
		float dfov_overlap = 2.f * atan(tan(vfov / 2.f) * (diagonal_aspect_overlap / verticalAspect));

		std::cout << "Target DFoV  " << to_degrees(target_dfov) << std::endl;
		std::cout << "Diagonal Aspect: " << diagonal_aspect << std::endl;
		std::cout << "HFoV Original: " << to_degrees(hfov_original) << std::endl;
		std::cout << "VFoV: " << to_degrees(vfov) << std::endl;
		std::cout << "hfov_overlap: " << to_degrees(hfov_overlap) << std::endl;
		std::cout << "aspect_overlap: " << aspect_overlap << std::endl;
		std::cout << "diagonalAspectOverlap: " << diagonal_aspect_overlap << std::endl;
		std::cout << "dfov_overlap: " << to_degrees(dfov_overlap) << std::endl;  

		std::cout << qlog(float4(0, 0, 0, 1)) << std::endl;
		std::cout << qexp(float4(0, 0, 0, 1)) << std::endl;
		auto r = make_rotation_quat_axis_angle({ 0, 1, 0 }, .707);
		std::cout << r << std::endl;
		auto p = qlog(r);
		std::cout << qexp(p) << std::endl;
		std::cout << qpow(r, 2.f) << std::endl;

		Pose a = Pose(r, { 0, 5, 0 });
		Pose b = Pose(make_rotation_quat_axis_angle({ 0, 1, 0 }, 3.14), { 0, 2, 0 });

		std::cout << make_pose_from_to(a, b) << std::endl;
		std::cout << a.inverse() * b << std::endl;

		std::cout << "Closeness: " << compute_quat_closeness(r, b.orientation) << std::endl;
    }
    
    void on_window_resize(int2 size) override
    {

    }
    
    void on_input(const InputEvent & event) override
    {
		if (igm) igm->update_input(event);

		if (event.type == InputEvent::MOUSE && event.is_mouse_down())
		{
		}
		else if (event.type == InputEvent::CURSOR && event.drag)
		{
		}
    }
    
    void on_update(const UpdateEvent & e) override
    {

    }
    
    void on_draw() override
    {
        glfwMakeContextCurrent(window);
        
        glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);

        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
     
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glClearColor(0.1f, 0.1f, 0.5f, 1.0f);

        const auto proj = camera.get_projection_matrix((float) width / (float) height);
        const float4x4 view = camera.get_view_matrix();
        const float4x4 viewProj = mul(proj, view);

        grid.render(proj, view);

		if (igm) igm->begin_frame();
		ImGui::Text("Lens Dev");
		if (igm) igm->end_frame();

        gl_check_error(__FILE__, __LINE__);
        
        glfwSwapBuffers(window);
        
        frameCount++;
    }
    
};