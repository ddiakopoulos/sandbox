#include "index.hpp"
#include "noise1234.h"

Geometry make_noisy_blob()
{
    Geometry blob = make_sphere(2.0f);
    for (auto & v : blob.vertices)
    {
        v *= 1.33f;
        float n = Noise1234::noise(v.x, v.y, v.z);
        v += (0.25f * n);
    }
    blob.compute_normals();
    return blob;
}

struct DecalVertex
{
    float3 v;
    float3 n;
    DecalVertex(float3 v, float3 n) : v(v), n(n) {}
    DecalVertex() {}
};

std::vector<DecalVertex> clip_face(std::vector<DecalVertex> & inVertices, float3 dimensions, float3 plane)
{
    float size = 0.5f * std::abs(dot(dimensions, plane));
    
    auto clip = [&](const DecalVertex v0, const DecalVertex v1, float3 p)
    {
        auto d0 = dot(v0.v, p) - size;
        auto d1 = dot(v1.v, p) - size;
        auto s = d0 / (d0 - d1);
        
        DecalVertex vert = {
            float3(v0.v.x + s * (v1.v.x - v0.v.x), v0.v.y + s * (v1.v.y - v0.v.y), v0.v.z + s * (v1.v.z - v0.v.z)),
            float3(v0.n.x + s * (v1.n.x - v0.n.x), v0.n.y + s * (v1.n.y - v0.n.y), v0.n.z + s * (v1.n.z - v0.n.z))
        };
        
        // need to clip more values (texture coordinates)? do it this way:
        //intersectpoint.value = a.value + s*(b.value-a.value);
        return vert;
    };
    
    std::vector<DecalVertex> outVertices;
    
    for (int j = 0; j < inVertices.size(); j += 3)
    {
        float d1 = dot(inVertices[j + 0].v, plane) - size;
        float d2 = dot(inVertices[j + 1].v, plane) - size;
        float d3 = dot(inVertices[j + 2].v, plane) - size;
        
        bool v1Out = d1 > 0.f;
        bool v2Out = d2 > 0.f;
        bool v3Out = d3 > 0.f;
        
        int total = (v1Out ? 1 : 0) + (v2Out ? 1 : 0) + (v3Out ? 1 : 0);
        
        switch(total)
        {
            case 0:
            {
                outVertices.push_back(inVertices[j + 0]);
                outVertices.push_back(inVertices[j + 1]);
                outVertices.push_back(inVertices[j + 2]);
                break;
            }
            case 1:
            {
                DecalVertex nV1, nV2, nV3, nV4;
                
                if (v1Out)
                {
                    nV1 = inVertices[j + 1];
                    nV2 = inVertices[j + 2];
                    nV3 = clip(inVertices[j], nV1, plane);
                    nV4 = clip(inVertices[j], nV2, plane);
                }
                
                if (v2Out)
                {
                    nV1 = inVertices[j + 0];
                    nV2 = inVertices[j + 2];
                    nV3 = clip(inVertices[j + 1], nV1, plane);
                    nV4 = clip(inVertices[j + 1], nV2, plane);
                    
                    outVertices.push_back(nV3);
                    outVertices.push_back(nV2);
                    outVertices.push_back(nV1);
                    
                    outVertices.push_back(nV2);
                    outVertices.push_back(nV3);
                    outVertices.push_back(nV4);
                    
                    break;
                }
                
                if (v3Out)
                {
                    nV1 = inVertices[j + 0];
                    nV2 = inVertices[j + 1];
                    nV3 = clip(inVertices[j + 2], nV1, plane);
                    nV4 = clip(inVertices[j + 2], nV2, plane);
                }
                
                outVertices.push_back(nV1);
                outVertices.push_back(nV2);
                outVertices.push_back(nV3);
                
                outVertices.push_back(nV4);
                outVertices.push_back(nV3);
                outVertices.push_back(nV2);
                
                break;
            }
            case 2:
            {
                DecalVertex nV1, nV2, nV3;
                
                if (!v1Out)
                {
                    nV1 = inVertices[j + 0];
                    nV2 = clip(nV1, inVertices[j + 1], plane);
                    nV3 = clip(nV1, inVertices[j + 2], plane);
                    outVertices.push_back(nV1);
                    outVertices.push_back(nV2);
                    outVertices.push_back(nV3);
                }
                
                if (!v2Out)
                {
                    nV1 = inVertices[j + 1];
                    nV2 = clip(nV1, inVertices[j + 2], plane);
                    nV3 = clip(nV1, inVertices[j], plane);
                    outVertices.push_back(nV1);
                    outVertices.push_back(nV2);
                    outVertices.push_back(nV3);
                }
                
                if (!v3Out)
                {
                    nV1 = inVertices[j + 2];
                    nV2 = clip(nV1, inVertices[j + 0], plane);
                    nV3 = clip(nV1, inVertices[j + 1], plane);
                    outVertices.push_back(nV1);
                    outVertices.push_back(nV2);
                    outVertices.push_back(nV3);
                }
                
                break;
            }
            case 3:
            {
                break;
            }
        }
        
    }
    
    return outVertices;
}

Geometry compute_decal(const Geometry & mesh, Pose meshPose, Pose cubePose, float3 dimensions, float3 check = float3(1, 1, 1))
{
    Geometry decal;
    
    std::vector<DecalVertex> finalVertices;

    for (int i = 0; i < mesh.faces.size(); i++)
    {
        uint3 f = mesh.faces[i];
        std::vector<DecalVertex> vertices;
        
        for (int j = 0; j < 3; j++)
        {
            float3 v = mesh.vertices[f[j]];
            float3 n = mesh.normals[f[j]];
            v = transform_coord(meshPose.matrix(), v);
            v = transform_coord(cubePose.inverse().matrix(), v);
            vertices.emplace_back(v, n);
        }
        
        if (check.x)
        {
            vertices = clip_face(vertices, dimensions, float3(1, 0, 0));
            vertices = clip_face(vertices, dimensions, float3(-1, 0, 0));
        }
        
        if (check.y)
        {
            vertices = clip_face(vertices, dimensions, float3(0, 1, 0));
            vertices = clip_face(vertices, dimensions, float3(0, -1, 0));
        }
        
        if (check.z)
        {
            vertices = clip_face(vertices, dimensions, float3(0, 0, 1));
            vertices = clip_face(vertices, dimensions, float3(0, 0, -1));
        }
        
        for (int j = 0; j < vertices.size(); j++)
        {
            auto & currentVertex = vertices[j];
            decal.texCoords.push_back(float2(0.5f + (currentVertex.v.x / dimensions.x), 0.5f + (currentVertex.v.y / dimensions.y)));
            currentVertex.v = transform_coord(cubePose.matrix(), currentVertex.v);
        }
        
        if (vertices.size() == 0)
            continue;
        
        finalVertices.insert(finalVertices.end(), vertices.begin(), vertices.end());
    }
    
    for (int k = 0; k < finalVertices.size(); k += 3)
    {
        decal.faces.emplace_back(k, k + 1, k + 2);
        
        decal.vertices.push_back(finalVertices[k + 0].v);
        decal.vertices.push_back(finalVertices[k + 1].v);
        decal.vertices.push_back(finalVertices[k + 2].v);
        
        decal.normals.push_back(finalVertices[k + 0].n);
        decal.normals.push_back(finalVertices[k + 1].n);
        decal.normals.push_back(finalVertices[k + 2].n);
    }
    
    return decal;
}

Geometry make_decal_geometry(Geometry & mesh, Pose meshPose, Pose cubePose, float3 dimensions)
{
    Geometry cube = make_cube();
    
    // Scale
    for (int i = 0; i < cube.vertices.size(); i++)
    {
        cube.vertices[i] *= dimensions;
    }
    
    return compute_decal(mesh, meshPose, cubePose, dimensions);
}

struct ExperimentalApp : public GLFWApp
{
    uint64_t frameCount = 0;

    GlCamera camera;
    HosekProceduralSky skydome;
    RenderableGrid grid;
    FlyCameraController cameraController;
    
    std::vector<Renderable> proceduralModels;
    std::vector<Renderable> decalModels;
    
    std::vector<LightObject> lights;
    
    std::unique_ptr<GlShader> simpleShader;
    
    GlTexture splatterTex;
    
    ExperimentalApp() : GLFWApp(1280, 720, "Sandbox App")
    {
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
        
        cameraController.set_camera(&camera);
        
        camera.look_at({0, 8, 24}, {0, 0, 0});

        simpleShader.reset(new GlShader(read_file_text("assets/shaders/simple_texture_vert.glsl"), read_file_text("assets/shaders/simple_texture_frag.glsl")));
        
        splatterTex = load_image("assets/images/splatter.png");
        
        {
            lights.resize(2);
            lights[0].color = float3(249.f / 255.f, 228.f / 255.f, 157.f / 255.f);
            lights[0].pose.position = float3(25, 15, 0);
            lights[1].color = float3(255.f / 255.f, 242.f / 255.f, 254.f / 255.f);
            lights[1].pose.position = float3(-25, 15, 0);
        }
        
        {
            proceduralModels.resize(4);
            
            proceduralModels[0] = Renderable(make_noisy_blob());
            proceduralModels[0].pose.position = float3(0, 2, +8);
            
            proceduralModels[1] = Renderable(make_cube());
            proceduralModels[1].pose.position = float3(0, 2, -8);
            
            proceduralModels[2] = Renderable(make_icosahedron());
            proceduralModels[2].pose.position = float3(8, 2, 0);
            
            proceduralModels[3] = Renderable(make_octohedron());
            proceduralModels[3].pose.position = float3(-8, 2, 0);
        }
        
        grid = RenderableGrid(1, 64, 64);
        
        gl_check_error(__FILE__, __LINE__);
    }
    
    void on_window_resize(int2 size) override
    {

    }
    
    void on_input(const InputEvent & event) override
    {
        cameraController.handle_input(event);
        
        if (event.type == InputEvent::KEY)
        {
            if (event.value[0] == GLFW_KEY_SPACE && event.action == GLFW_RELEASE)
            {
                // todo
            }
        }
        
        if (event.type == InputEvent::MOUSE && event.action == GLFW_PRESS)
        {
            if (event.value[0] == GLFW_MOUSE_BUTTON_LEFT)
            {
                for (auto & model : proceduralModels)
                {
                    auto worldRay = camera.get_world_ray(event.cursor, float2(event.windowSize));
                    if (model.check_hit(worldRay))
                    {
                        // todo
                    }
                }

            }
        }
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
        
        skydome.render(viewProj, camera.get_eye_point(), camera.farClip);
        
        // Simple Shader
        {
            simpleShader->bind();
            
            simpleShader->uniform("u_viewProj", viewProj);
            simpleShader->uniform("u_eye", camera.get_eye_point());
            
            simpleShader->uniform("u_emissive", float3(.10f, 0.10f, 0.10f));
            simpleShader->uniform("u_diffuse", float3(0.4f, 0.4f, 0.4f));
            
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
            
            for (const auto & decal : decalModels)
            {
                simpleShader->uniform("u_modelMatrix", decal.get_model());
                simpleShader->uniform("u_modelMatrixIT", inv(transpose(decal.get_model())));
                simpleShader->texture("u_diffuseTex", 0, splatterTex);
                decal.draw();
            }
            
            gl_check_error(__FILE__, __LINE__);
            
            simpleShader->unbind();
        }
        
        grid.render(proj, view);

        gl_check_error(__FILE__, __LINE__);
        
        glfwSwapBuffers(window);
        
        frameCount++;
    }
    
};
