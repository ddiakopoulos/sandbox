#include "index.hpp"

constexpr const char basic_wireframe_vert[] = R"(#version 330
    layout(location = 0) in vec3 vertex;
    layout(location = 2) in vec3 inColor;
    uniform mat4 u_mvp;
    out vec3 color;
    void main()
    {
        gl_Position = u_mvp * vec4(vertex.xyz, 1);
        color = inColor;
    }
)";

constexpr const char basic_wireframe_frag[] = R"(#version 330
    in vec3 color;
    out vec4 f_color;
    void main()
    {
        f_color = vec4(color.rgb, 1);
    }
)";

// A value type representing an abstract direction vector in 3D space, independent of any coordinate system
enum class coord_axis { forward, back, left, right, up, down };

inline float dot(coord_axis a, coord_axis b)
{
    static float table[6][6]{ { +1,-1,0,0,0,0 },{ -1,+1,0,0,0,0 },{ 0,0,+1,-1,0,0 },{ 0,0,-1,+1,0,0 },{ 0,0,0,0,+1,-1 },{ 0,0,0,0,-1,+1 } };
    return table[static_cast<int>(a)][static_cast<int>(b)];
}

// A concrete 3D coordinate system with defined x, y, and z axes
struct coord_system
{
    coord_axis x_axis, y_axis, z_axis;
    float3 get_axis(coord_axis a) const { return{ dot(x_axis, a), dot(y_axis, a), dot(z_axis, a) }; }
    float3 get_left() const { return get_axis(coord_axis::left); }
    float3 get_right() const { return get_axis(coord_axis::right); }
    float3 get_up() const { return get_axis(coord_axis::up); }
    float3 get_down() const { return get_axis(coord_axis::down); }
    float3 get_forward() const { return get_axis(coord_axis::forward); }
    float3 get_back() const { return get_axis(coord_axis::back); }
};

inline float3x3 make_transform(const coord_system & from, const coord_system & to) { return{ to.get_axis(from.x_axis), to.get_axis(from.y_axis), to.get_axis(from.z_axis) }; }
inline float4x4 make_transform_4x4(const coord_system & from, const coord_system & to) { return{ { to.get_axis(from.x_axis),0 },{ to.get_axis(from.y_axis),0 },{ to.get_axis(from.z_axis),0 },{ 0,0,0,1 } }; }

inline Geometry coordinate_system_geometry()
{
    coord_system opengl_coords { coord_axis::right, coord_axis::up, coord_axis::back }; // traditional rh opengl

    Geometry axis;

    for (auto a : { opengl_coords.get_right(), opengl_coords.get_up(),  opengl_coords.get_back() })
    {
        axis.vertices.emplace_back(0.f, 0.f, 0.f);
        axis.vertices.emplace_back(a);

        axis.colors.emplace_back(a, 1.f);
        axis.colors.emplace_back(a, 1.f);
    }

    return axis;
}

inline GlMesh make_coordinate_system_mesh()
{
    auto m = make_mesh_from_geometry(coordinate_system_geometry());
    m.set_non_indexed(GL_LINES);
    return m;
}

struct ExperimentalApp : public GLFWApp
{
    std::unique_ptr<GlShader> wireframeShader;

    GlCamera debugCamera;
    FlyCameraController cameraController;

    GlMesh headMesh, cameraMesh;
    Pose camera;

    float rotation = 0.f;

    ExperimentalApp() : GLFWApp(1200, 1200, "Nearly Empty App")
    {
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
        gl_check_error(__FILE__, __LINE__);

        wireframeShader.reset(new GlShader(basic_wireframe_vert, basic_wireframe_frag));

        headMesh = make_coordinate_system_mesh();
        cameraMesh = make_frustum_mesh(1.0);

        camera.position = float3(0, 1.75, 0.5f);

        debugCamera.look_at({0, 3.0, -3.5}, {0, 2.0, 0});
        cameraController.set_camera(&debugCamera);
    }
    
    void on_window_resize(int2 size) override
    {

    }
    
    void on_input(const InputEvent & event) override
    {
        cameraController.handle_input(event);
    }
    
    void on_update(const UpdateEvent & e) override
    {
        cameraController.update(e.timestep_ms);
        rotation += 0.001f;
        camera.orientation = make_rotation_quat_axis_angle({ 0, 1, 0 }, rotation);
    }
    
    void on_draw() override
    {
        glfwMakeContextCurrent(window);
        glfwSwapInterval(1);
        
        glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);

        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
     
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glClearColor(0.2f, 0.2f, 0.2f, 1.0f);

        const auto proj = debugCamera.get_projection_matrix((float) width / (float) height);
        const float4x4 view = debugCamera.get_view_matrix();
        const float4x4 viewProj = mul(proj, view);

        wireframeShader->bind();

        Pose offset = Pose(float4(0, 0, 0, 1), float3(0, 0, -0.5f));

        auto cameraMatrix = mul(viewProj, camera.matrix());
        wireframeShader->uniform("u_mvp", cameraMatrix);
        cameraMesh.draw_elements();

        auto headMatrix = mul(viewProj, Pose(camera * offset).matrix());
        wireframeShader->uniform("u_mvp", headMatrix);
        headMesh.draw_elements();

        wireframeShader->unbind();

        gl_check_error(__FILE__, __LINE__);
        
        glfwSwapBuffers(window);
    }
    
};