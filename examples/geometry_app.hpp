#include "anvil.hpp"
#include "tiny_obj_loader.h"

using namespace math;
using namespace util;
using namespace gfx;
    
constexpr const char colorVertexShader[] = R"(#version 330
    layout(location = 0) in vec3 vertex;
    layout(location = 1) in vec3 vnorm;
    uniform mat4 u_modelMatrix;
    uniform mat4 u_modelMatrixIT;
    uniform mat4 u_viewProj;
    uniform vec3 u_color;
    out vec3 color;
    out vec3 normal;
    void main()
    {
        vec4 worldPos = u_modelMatrix * vec4(vertex, 1);
        gl_Position = u_viewProj * worldPos;
        color = u_color * 0.80;
        normal = normalize((u_modelMatrixIT * vec4(vnorm,0)).xyz);
    }
)";

constexpr const char colorFragmentShader[] = R"(#version 330
    in vec3 color;
    out vec4 f_color;
    in vec3 normal;
    void main()
    {
        f_color = (vec4(color.rgb, 1) * 0.75)+ (dot(normal, vec3(0, 1, 0)) * 0.33);
    }
)";

struct ExperimentalApp : public GLFWApp
{
    uint64_t frameCount = 0;

    GlCamera camera;
    PreethamProceduralSky preethamSky;
    RenderableGrid grid;
    FPSCameraController cameraController;
    
    std::unique_ptr<GizmoEditor> gizmoEditor;
    
    std::vector<Renderable> proceduralModels;
    std::vector<Renderable> debugModels;
    
    std::unique_ptr<GlShader> simpleShader;
    std::unique_ptr<GlShader> colorShader;
    
    std::vector<LightObject> lights;
    
    std::vector<GlMesh> sponzaMeshes;

    ExperimentalApp() : GLFWApp(820, 480, "Geometry App")
    {
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
        grid = RenderableGrid(1, 100, 100);
        cameraController.set_camera(&camera);
        
        gizmoEditor.reset(new GizmoEditor(camera));
        
        simpleShader.reset(new gfx::GlShader(read_file_text("assets/shaders/simple_vert.glsl"), read_file_text("assets/shaders/simple_frag.glsl")));
        colorShader.reset(new gfx::GlShader(colorVertexShader, colorFragmentShader));
        
        {
            std::vector<tinyobj::shape_t> shapes;
            std::vector<tinyobj::material_t> materials;
            std::string err;
            bool status = tinyobj::LoadObj(shapes, materials, err, "assets/models/sponza/sponza.obj", "assets/models/sponza/");
            
            if (!err.empty())
            {
                std::cerr << err << std::endl;
            }
            
            std::vector<Geometry> geometries;
            
            std::cout << "# of shapes    : " << shapes.size() << std::endl;
            std::cout << "# of materials : " << materials.size() << std::endl;
            
            // Parse tinyobj data into geometry struct
            for (unsigned int i = 0; i < shapes.size(); i++)
            {
                Geometry g;
                tinyobj::shape_t *shape = &shapes[i];
                tinyobj::mesh_t *mesh = &shapes[i].mesh;
                
                std::cout << "Parsing: " << shape->name << std::endl;
                std::cout << mesh->indices.size() << std::endl;
                for (size_t i = 0; i < mesh->indices.size(); i += 3)
                {
                    uint32_t idx1 = mesh->indices[i + 0];
                    uint32_t idx2 = mesh->indices[i + 1];
                    uint32_t idx3 = mesh->indices[i + 2];
                    g.faces.push_back({idx1, idx2, idx3});
                }
                
                for (size_t v = 0; v < mesh->positions.size(); v += 3)
                {
                    float3 vert = float3(mesh->positions[v + 0], mesh->positions[v + 1], mesh->positions[v + 2]);
                    g.vertices.push_back(vert);
                    
                }
            
                geometries.push_back(g);
                
            }
            
            for (auto & g : geometries)
            {
                g.compute_normals();
                //sponzaMeshes.push_back(make_mesh_from_geometry(g));
            }
            
        }
        {
            lights.resize(2);
            
            lights[0].color = float3(60.f / 255.f, 168.f / 255.f, 180.f / 255.f);
            lights[0].pose.position = float3(25, 15, 0);
            
            lights[1].color = float3(100.f / 255.f, 120.f / 255.f, 105.f / 255.f);
            lights[1].pose.position = float3(-25, 15, 0);
        }
        
        {
            proceduralModels.resize(3);
            
            proceduralModels[0] = Renderable(make_sphere(1.0));
            proceduralModels[0].pose.position = float3(0, 0, +5);
            
            proceduralModels[1] = Renderable(make_cube());
            proceduralModels[1].pose.position = float3(0, 0, -5);
        }
        
        gfx::gl_check_error(__FILE__, __LINE__);
    }
    
    void on_window_resize(math::int2 size) override
    {

    }
    
    void on_input(const InputEvent & event) override
    {
        gizmoEditor->handle_input(event, proceduralModels);
        cameraController.handle_input(event);
    }
    
    void on_update(const UpdateEvent & e) override
    {
        cameraController.update(e.timestep_ms);
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
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);

        const auto proj = camera.get_projection_matrix((float) width / (float) height);
        const float4x4 view = camera.get_view_matrix();
        const float4x4 viewProj = mul(proj, view);
        
        preethamSky.render(viewProj, camera.get_eye_point(), camera.farClip);
        
        // Simple Shader
        {
            simpleShader->bind();
            
            simpleShader->uniform("u_viewProj", viewProj);
            simpleShader->uniform("u_eye", camera.get_eye_point());
            
            simpleShader->uniform("u_emissive", float3(.10f, 0.10f, 0.10f));
            simpleShader->uniform("u_diffuse", float3(0.4f, 0.4f, 0.25f));
            
            for (int i = 0; i < lights.size(); i++)
            {
                auto light = lights[i];
                
                simpleShader->uniform("u_lights[" + std::to_string(i) + "].position", light.pose.position);
                simpleShader->uniform("u_lights[" + std::to_string(i) + "].color", light.color);
            }

            for (const auto & model : proceduralModels)
            {
                simpleShader->uniform("u_modelMatrix", model.get_model());
                simpleShader->uniform("u_modelMatrixIT", inv(transpose(model.get_model())));
                model.draw();
            }
            
            for (const auto & model : sponzaMeshes)
            {
                Pose p;
                auto modelMat = p.matrix();
                simpleShader->uniform("u_modelMatrix", modelMat);
                simpleShader->uniform("u_modelMatrixIT", inv(transpose(modelMat)));
                model.draw_elements();
                
            }
            
            simpleShader->unbind();
        }
        
        // Color gizmo shader
        {
            glEnable(GL_POLYGON_OFFSET_LINE);
            glPolygonOffset(-1, -1);
            
            colorShader->bind();
            
            colorShader->uniform("u_viewProj", viewProj);
            
            if (gizmoEditor->get_selected_object())
            {
                Renderable * selectedObject = gizmoEditor->get_selected_object();
                
                for (auto axis : {float3(1, 0, 0), float3(0, 1, 0), float3(0, 0, 1)})
                {
                    auto p = selectedObject->pose * Pose(make_rotation_quat_between_vectors({1,0,0}, axis), {0,0,0});
                    colorShader->uniform("u_modelMatrix", p.matrix());
                    colorShader->uniform("u_modelMatrixIT", inv(transpose(p.matrix())));
                    colorShader->uniform("u_color", axis);
                    gizmoEditor->get_gizmo_mesh().draw();
                }
            }

            colorShader->unbind();
        }
        
        grid.render(proj, view, {0, -0.5, 0});

        gfx::gl_check_error(__FILE__, __LINE__);
        
        glfwSwapBuffers(window);
        
        frameCount++;
    }
    
};
