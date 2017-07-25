#include "index.hpp"
#include "workbench.hpp"
#include "imgui/imgui_internal.h"
#include "skeleton.hpp"

using namespace avl;

#include <xmmintrin.h>
#include <immintrin.h>

struct IKChain
{
    Pose root;
    Pose joint;
    Pose end;
};

tinygizmo::rigid_transform root_transform;
tinygizmo::rigid_transform joint_transform;
tinygizmo::rigid_transform end_transform;
tinygizmo::rigid_transform target_transform;

HumanSkeleton skeleton;

static inline __m128 linearcombine_sse2(const __m128 & a, const float4x4 & B)
{
    __m128 result;
    result = _mm_mul_ps(_mm_shuffle_ps(a, a, 0x00),                    *reinterpret_cast<const __m128*>(&B[0]));
    result = _mm_add_ps(result, _mm_mul_ps(_mm_shuffle_ps(a, a, 0x55), *reinterpret_cast<const __m128*>(&B[1])));
    result = _mm_add_ps(result, _mm_mul_ps(_mm_shuffle_ps(a, a, 0xaa), *reinterpret_cast<const __m128*>(&B[2])));
    result = _mm_add_ps(result, _mm_mul_ps(_mm_shuffle_ps(a, a, 0xff), *reinterpret_cast<const __m128*>(&B[3])));
    return result;
}


inline float4x4 multiply(const float4x4 & A, const float4x4 & B)
{
    // out_ij = sum_k a_ik b_kj => out_0j = a_00 * b_0j + a_01 * b_1j + a_02 * b_2j + a_03 * b_3j
    const __m128 out0x = linearcombine_sse2(*reinterpret_cast<const __m128*>(&A[0]), B);
    const __m128 out1x = linearcombine_sse2(*reinterpret_cast<const __m128*>(&A[1]), B);
    const __m128 out2x = linearcombine_sse2(*reinterpret_cast<const __m128*>(&A[2]), B);
    const __m128 out3x = linearcombine_sse2(*reinterpret_cast<const __m128*>(&A[3]), B);

    float4x4 out;
    out[0] = *reinterpret_cast<const float4*>(&out0x);
    out[1] = *reinterpret_cast<const float4*>(&out1x);
    out[2] = *reinterpret_cast<const float4*>(&out2x);
    out[3] = *reinterpret_cast<const float4*>(&out3x);

    return out;
}

inline void invert_sse(float4x4 & matrix)
{
    float * src = &(matrix[0][0]);

    __m128 minor0{}, minor1{}, minor2{}, minor3{};
    __m128 row0{}, row1{}, row2{}, row3{};
    __m128 det{}, tmp1{};

    tmp1 = _mm_loadh_pi(_mm_loadl_pi(tmp1, (__m64*)(src)), (__m64*)(src + 4));
    row1 = _mm_loadh_pi(_mm_loadl_pi(row1, (__m64*)(src + 8)), (__m64*)(src + 12));

    row0 = _mm_shuffle_ps(tmp1, row1, 0x88);
    row1 = _mm_shuffle_ps(row1, tmp1, 0xDD);

    tmp1 = _mm_loadh_pi(_mm_loadl_pi(tmp1, (__m64*)(src + 2)), (__m64*)(src + 6));
    row3 = _mm_loadh_pi(_mm_loadl_pi(row3, (__m64*)(src + 10)), (__m64*)(src + 14));

    row2 = _mm_shuffle_ps(tmp1, row3, 0x88);
    row3 = _mm_shuffle_ps(row3, tmp1, 0xDD);

    tmp1 = _mm_mul_ps(row2, row3);
    tmp1 = _mm_shuffle_ps(tmp1, tmp1, 0xB1);

    minor0 = _mm_mul_ps(row1, tmp1);
    minor1 = _mm_mul_ps(row0, tmp1);

    tmp1 = _mm_shuffle_ps(tmp1, tmp1, 0x4E);

    minor0 = _mm_sub_ps(_mm_mul_ps(row1, tmp1), minor0);
    minor1 = _mm_sub_ps(_mm_mul_ps(row0, tmp1), minor1);
    minor1 = _mm_shuffle_ps(minor1, minor1, 0x4E);

    tmp1 = _mm_mul_ps(row1, row2);
    tmp1 = _mm_shuffle_ps(tmp1, tmp1, 0xB1);

    minor0 = _mm_add_ps(_mm_mul_ps(row3, tmp1), minor0);
    minor3 = _mm_mul_ps(row0, tmp1);

    tmp1 = _mm_shuffle_ps(tmp1, tmp1, 0x4E);

    minor0 = _mm_sub_ps(minor0, _mm_mul_ps(row3, tmp1));
    minor3 = _mm_sub_ps(_mm_mul_ps(row0, tmp1), minor3);
    minor3 = _mm_shuffle_ps(minor3, minor3, 0x4E);

    tmp1 = _mm_mul_ps(_mm_shuffle_ps(row1, row1, 0x4E), row3);
    tmp1 = _mm_shuffle_ps(tmp1, tmp1, 0xB1);
    row2 = _mm_shuffle_ps(row2, row2, 0x4E);

    minor0 = _mm_add_ps(_mm_mul_ps(row2, tmp1), minor0);
    minor2 = _mm_mul_ps(row0, tmp1);

    tmp1 = _mm_shuffle_ps(tmp1, tmp1, 0x4E);

    minor0 = _mm_sub_ps(minor0, _mm_mul_ps(row2, tmp1));
    minor2 = _mm_sub_ps(_mm_mul_ps(row0, tmp1), minor2);
    minor2 = _mm_shuffle_ps(minor2, minor2, 0x4E);

    tmp1 = _mm_mul_ps(row0, row1);
    tmp1 = _mm_shuffle_ps(tmp1, tmp1, 0xB1);

    minor2 = _mm_add_ps(_mm_mul_ps(row3, tmp1), minor2);
    minor3 = _mm_sub_ps(_mm_mul_ps(row2, tmp1), minor3);

    tmp1 = _mm_shuffle_ps(tmp1, tmp1, 0x4E);

    minor2 = _mm_sub_ps(_mm_mul_ps(row3, tmp1), minor2);
    minor3 = _mm_sub_ps(minor3, _mm_mul_ps(row2, tmp1));

    tmp1 = _mm_mul_ps(row0, row3);
    tmp1 = _mm_shuffle_ps(tmp1, tmp1, 0xB1);

    minor1 = _mm_sub_ps(minor1, _mm_mul_ps(row2, tmp1));
    minor2 = _mm_add_ps(_mm_mul_ps(row1, tmp1), minor2);

    tmp1 = _mm_shuffle_ps(tmp1, tmp1, 0x4E);

    minor1 = _mm_add_ps(_mm_mul_ps(row2, tmp1), minor1);
    minor2 = _mm_sub_ps(minor2, _mm_mul_ps(row1, tmp1));

    tmp1 = _mm_mul_ps(row0, row2);
    tmp1 = _mm_shuffle_ps(tmp1, tmp1, 0xB1);

    minor1 = _mm_add_ps(_mm_mul_ps(row3, tmp1), minor1);
    minor3 = _mm_sub_ps(minor3, _mm_mul_ps(row1, tmp1));

    tmp1 = _mm_shuffle_ps(tmp1, tmp1, 0x4E);

    minor1 = _mm_sub_ps(minor1, _mm_mul_ps(row3, tmp1));
    minor3 = _mm_add_ps(_mm_mul_ps(row1, tmp1), minor3);

    det = _mm_mul_ps(row0, minor0);
    det = _mm_add_ps(_mm_shuffle_ps(det, det, 0x4E), det);
    det = _mm_add_ss(_mm_shuffle_ps(det, det, 0xB1), det);
    tmp1 = _mm_rcp_ss(det);

    det = _mm_sub_ss(_mm_add_ss(tmp1, tmp1), _mm_mul_ss(det, _mm_mul_ss(tmp1, tmp1)));
    det = _mm_shuffle_ps(det, det, 0x00);

    minor0 = _mm_mul_ps(det, minor0);
    _mm_storel_pi((__m64*)(src), minor0);
    _mm_storeh_pi((__m64*)(src + 2), minor0);

    minor1 = _mm_mul_ps(det, minor1);
    _mm_storel_pi((__m64*)(src + 4), minor1);
    _mm_storeh_pi((__m64*)(src + 6), minor1);

    minor2 = _mm_mul_ps(det, minor2);
    _mm_storel_pi((__m64*)(src + 8), minor2);
    _mm_storeh_pi((__m64*)(src + 10), minor2);

    minor3 = _mm_mul_ps(det, minor3);
    _mm_storel_pi((__m64*)(src + 12), minor3);
    _mm_storeh_pi((__m64*)(src + 14), minor3);
}


/*
// Make a cylinder with the bottom center at 0 and up on the +Y axis
inline Geometry make_cylinder(const float height, const float radiusTop, const float radiusBottom, const glm::vec4& colorTop, const glm::vec4& colorBottom, const int segments)
{
    vertex_positions.empty();
    vertex_colors.empty();
    nbSegments = segments;

    double angle = 0.0;
    vertex_positions.push_back(glm::vec4(0, height, 0, 1));
    vertex_colors.push_back(colorTop);
    for (unsigned int i = 0; i<nbSegments; ++i)
    {
        angle = ((double)i) / ((double)nbSegments)*2.0*3.14;
        vertex_positions.push_back(glm::vec4(radiusTop*std::cos(angle), height, radiusTop*std::sin(angle), 1.0));
        vertex_colors.push_back(colorTop);
        vertex_indexes.push_back(0);
        vertex_indexes.push_back((i + 1) % nbSegments + 1);
        vertex_indexes.push_back(i + 1);
    }

    vertex_positions.push_back(glm::vec4(0, 0, 0, 1));
    vertex_colors.push_back(colorBottom);
    for (unsigned int i = 0; i<nbSegments; ++i)
    {
        angle = ((double)i) / ((double)nbSegments)*2.0*3.14;
        vertex_positions.push_back(glm::vec4(radiusBottom*std::cos(angle), 0.0, radiusBottom*std::sin(angle), 1.0));
        vertex_colors.push_back(colorBottom);
        vertex_indexes.push_back(nbSegments + 1);
        vertex_indexes.push_back(nbSegments + 2 + (i + 1) % nbSegments);
        vertex_indexes.push_back(nbSegments + i + 2);
    }

    for (unsigned int i = 0; i<nbSegments; ++i)
    {
        vertex_indexes.push_back(i + 1);
        vertex_indexes.push_back((i + 1) % nbSegments + 1);
        vertex_indexes.push_back(nbSegments + 2 + (i + 1) % nbSegments);

        vertex_indexes.push_back(i + 1);
        vertex_indexes.push_back(nbSegments + 2 + (i + 1) % nbSegments);
        vertex_indexes.push_back(nbSegments + i + 2);
    }

}
*/

shader_workbench::shader_workbench() : GLFWApp(1200, 800, "Shader Workbench")
{
    int width, height;
    glfwGetWindowSize(window, &width, &height);
    glViewport(0, 0, width, height);

    igm.reset(new gui::ImGuiManager(window));
    gui::make_dark_theme();

    normalDebug = shaderMonitor.watch("../assets/shaders/normal_debug_vert.glsl", "../assets/shaders/normal_debug_frag.glsl");

    sphere_mesh = make_sphere_mesh(0.1f);
    cylinder_mesh = make_mesh_from_geometry(make_tapered_capsule());

    make_sphere_mesh(0.25f); //make_cylinder_mesh(0.1, 0.2, 0.2, 32, 32, false);
    //cylinder_mesh.set_non_indexed(GL_LINES);

    gizmo.reset(new GlGizmo());

    root_transform.position.y = 1.f;
    joint_transform.position.y = 0.5f;
    joint_transform.position.z = -0.15f;
    end_transform.position.y = 0.f;

    target_transform.position = reinterpret_cast<const minalg::float3 &>(skeleton.bones[0].local_pose);

    cam.look_at({ 0, 9.5f, -6.0f }, { 0, 0.1f, 0 });
    flycam.set_camera(&cam);

    traverse_joint_chain(13, skeleton.bones);

    float angle = 0.1f;

    UniformRandomGenerator gen;

    Pose p;     

    CircularBuffer<float> avg_normal(100000);
    CircularBuffer<float> avg_optimized(100000);

    manual_timer t;

    auto what = Identity4x4;

    int i = 0;
    while (i < 100000)
    {
        p.position = { gen.random_float(), gen.random_float(), gen.random_float() };
        angle += 0.1f;
        p.orientation = make_rotation_quat_axis_angle({ 0, 1, 0 }, 0.5f);

        auto mat = p.matrix();

        {
            t.start();
            auto r = mul(mat, what);
            t.stop();
            avg_normal.put(t.get());
            //std::cout << "Normal: " << r << std::endl;
        }

        {
            t.start();
            auto r = multiply(mat, what);
            t.stop();
            avg_optimized.put(t.get());
            //std::cout << "Optim: " << r << std::endl;
        }

        //std::cout << "-------------------------- \n";
        ++i;
    }

    std::cout << "Normal: " << compute_mean(avg_normal) * 1000 << std::endl;
    std::cout << "Optimized: " << compute_mean(avg_optimized) * 1000 << std::endl;
}

shader_workbench::~shader_workbench()
{

}

void shader_workbench::on_window_resize(int2 size)
{

}

void shader_workbench::on_input(const InputEvent & event)
{
    igm->update_input(event);
    flycam.handle_input(event);

    if (event.type == InputEvent::KEY)
    {
        if (event.value[0] == GLFW_KEY_ESCAPE && event.action == GLFW_RELEASE) exit();
    }

    if (gizmo) gizmo->handle_input(event);
}

void shader_workbench::on_update(const UpdateEvent & e)
{
    flycam.update(e.timestep_ms);
    shaderMonitor.handle_recompile();
    elapsedTime += e.timestep_ms;
}

void shader_workbench::on_draw()
{
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    int width, height;
    glfwGetWindowSize(window, &width, &height);

    const float4x4 projectionMatrix = cam.get_projection_matrix(float(width) / float(height));
    const float4x4 viewMatrix = cam.get_view_matrix();
    const float4x4 viewProjectionMatrix = mul(projectionMatrix, viewMatrix);

    if (gizmo) gizmo->update(cam, float2(width, height));

    //tinygizmo::transform_gizmo("root", gizmo->gizmo_ctx, root_transform);
    //tinygizmo::transform_gizmo("joint", gizmo->gizmo_ctx, joint_transform);
    //tinygizmo::transform_gizmo("end", gizmo->gizmo_ctx, end_transform);
    tinygizmo::transform_gizmo("target", gizmo->gizmo_ctx, target_transform);

    const float3 rootPosition = reinterpret_cast<const float3 &>(root_transform.position);
    const float3 jointPosition = reinterpret_cast<const float3 &>(joint_transform.position);
    const float3 endPosition = reinterpret_cast<const float3 &>(end_transform.position);

    const float3 jointTarget = float3(0, 0, 0);
    const float3 effectorPosition = reinterpret_cast<const float3 &>(target_transform.position);

    float3 outJointPosition;
    float3 outEndPosition;

    SolveTwoBoneIK(rootPosition, jointPosition, endPosition, jointTarget, effectorPosition, outJointPosition, outEndPosition, false, 1.f, 1.f);

    float4x4 rootMatrix = reinterpret_cast<const float4x4 &>(root_transform.matrix());
    float4x4 jointMatrix = reinterpret_cast<const float4x4 &>(joint_transform.matrix());
    float4x4 endMatrix = reinterpret_cast<const float4x4 &>(end_transform.matrix());

    float4x4 outJointMatrix = mul(make_translation_matrix(outJointPosition), make_scaling_matrix(0.5));
    float4x4 outEffectorMatrix = mul(make_translation_matrix(outEndPosition), make_scaling_matrix(0.5));

    gpuTimer.start();

    // Main Scene
    {
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);

        glViewport(0, 0, width, height);
        glClearColor(1.f, 1.f, 1.f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        normalDebug->bind();
        normalDebug->uniform("u_viewProj", viewProjectionMatrix);

        // Some debug models
        /*
        for (const auto & model : { rootMatrix, jointMatrix, endMatrix, outJointMatrix, outEffectorMatrix })
        {
            normalDebug->uniform("u_modelMatrix", model);
            normalDebug->uniform("u_modelMatrixIT", inv(transpose(model)));
            sphere_mesh.draw_elements();
        }
        */

        skeleton.bones[0].local_pose = reinterpret_cast<const float4x4 &>(target_transform.matrix());

        auto skeletonBones = compute_static_pose(skeleton.bones);

        for (const auto & bone : skeletonBones)
        {
            normalDebug->uniform("u_modelMatrix", bone);
            normalDebug->uniform("u_modelMatrixIT", inv(transpose(bone)));
            cylinder_mesh.draw_elements();
        }

        normalDebug->unbind();
    }

    gpuTimer.stop();

    igm->begin_frame();
    // ... 
    igm->end_frame();

    if (gizmo) gizmo->draw();

    gl_check_error(__FILE__, __LINE__);

    glfwSwapBuffers(window);
}

IMPLEMENT_MAIN(int argc, char * argv[])
{
    try
    {
        shader_workbench app;
        app.main_loop();
    }
    catch (const std::exception & e)
    {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
