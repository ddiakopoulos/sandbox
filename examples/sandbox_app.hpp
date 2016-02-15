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

// http://blog.wolfire.com/2009/06/how-to-project-decals/

std::vector<DecalVertex> clip_face(const std::vector<DecalVertex> & inVertices, float3 dimensions, float3 plane)
{
    std::vector<DecalVertex> outVertices;
    
    float size = 0.5f * std::abs(dot(dimensions, plane));
    
    auto clip = [&](const DecalVertex v0, const DecalVertex v1, const float3 p)
    {
        auto d0 = dot(v0.v, p) - size;
        auto d1 = dot(v1.v, p) - size;
        auto s = d0 / (d0 - d1);
        
        DecalVertex vert = {
            float3(v0.v.x + s * (v1.v.x - v0.v.x), v0.v.y + s * (v1.v.y - v0.v.y), v0.v.z + s * (v1.v.z - v0.v.z)),
            float3(v0.n.x + s * (v1.n.x - v0.n.x), v0.n.y + s * (v1.n.y - v0.n.y), v0.n.z + s * (v1.n.z - v0.n.z))
        };
        
        return vert;
    };
    
    for (int j = 0; j < inVertices.size(); j += 3)
    {
        float d1 = dot(inVertices[j + 0].v, plane) - size;
        float d2 = dot(inVertices[j + 1].v, plane) - size;
        float d3 = dot(inVertices[j + 2].v, plane) - size;
        
        int v1Out = d1 > 0.f;
        int v2Out = d2 > 0.f;
        int v3Out = d3 > 0.f;
        
        int total = v1Out + v2Out + v3Out;

        DecalVertex nV1, nV2, nV3, nV4;
        
        switch (total)
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

Geometry make_decal_geometry(const Renderable & r, Pose cubePose, float3 dimensions)
{
    Geometry decal;
    std::vector<DecalVertex> finalVertices;
    
    auto & mesh = r.geom;
    assert(mesh.normals.size() > 0);
    
    for (int i = 0; i < mesh.faces.size(); i++)
    {
        uint3 f = mesh.faces[i];
        std::vector<DecalVertex> clippedVertices;
        
        for (int j = 0; j < 3; j++)
        {
            float3 v = mesh.vertices[f[j]];
            float3 n = mesh.normals[f[j]];
            v = transform_coord(r.pose.matrix(), v); // local into world
            v = transform_coord(cubePose.inverse().matrix(), v); // with the box
            clippedVertices.emplace_back(v, n);
        }

        // Clip X faces
        clippedVertices = clip_face(clippedVertices, dimensions, float3(1, 0, 0));
        clippedVertices = clip_face(clippedVertices, dimensions, float3(-1, 0, 0));
        
        // Clip Y faces
        clippedVertices = clip_face(clippedVertices, dimensions, float3(0, 1, 0));
        clippedVertices = clip_face(clippedVertices, dimensions, float3(0, -1, 0));

        // Clip Z faces
        clippedVertices = clip_face(clippedVertices, dimensions, float3(0, 0, 1));
        clippedVertices = clip_face(clippedVertices, dimensions, float3(0, 0, -1));
        
        // Projected coordinates are also our texCoords
        for (int v = 0; v < clippedVertices.size(); v++)
        {
            auto & a = clippedVertices[v];
            decal.texCoords.push_back(float2(0.5f + (a.v.x / dimensions.x), 0.5f + (a.v.y / dimensions.y)));
            a.v = transform_coord(cubePose.matrix(), a.v); // back to local
        }
        
        if (clippedVertices.size() == 0)
            continue;
        
        finalVertices.insert(finalVertices.end(), clippedVertices.begin(), clippedVertices.end());
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
    
    GlTexture anvilTex;
    GlTexture emptyTex;
    
    ExperimentalApp() : GLFWApp(1280, 720, "Sandbox App")
    {
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
        
        cameraController.set_camera(&camera);
        
        camera.look_at({0, 8, 24}, {0, 0, 0});

        simpleShader.reset(new GlShader(read_file_text("assets/shaders/simple_texture_vert.glsl"), read_file_text("assets/shaders/simple_texture_frag.glsl")));
        
        anvilTex = load_image("assets/images/uv_grid.png");
        
        std::vector<uint8_t> pixel = {255, 255, 255, 255};
        emptyTex.load_data(1, 1, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
        
        {
            lights.resize(2);
            lights[0].color = float3(249.f / 255.f, 228.f / 255.f, 157.f / 255.f);
            lights[0].pose.position = float3(25, 15, 0);
            lights[1].color = float3(255.f / 255.f, 242.f / 255.f, 254.f / 255.f);
            lights[1].pose.position = float3(-25, 15, 0);
        }
        
        {
            proceduralModels.resize(4);
            
            proceduralModels[0] = Renderable(make_torus());
            proceduralModels[0].pose.position = float3(0, 2, +8);
            
            proceduralModels[1] = Renderable(make_cube());
            proceduralModels[1].pose.position = float3(0, 2, -8);
            
            auto hollowCube = load_geometry_from_ply("assets/models/geometry/CubeHollowOpen.ply");
            for (auto & v : hollowCube.vertices) v *= 0.0125f;
            
            proceduralModels[2] = Renderable(hollowCube);
            proceduralModels[2].pose.position = float3(8, 2, 0);
            
            auto leePerryHeadModel = load_geometry_from_obj_no_texture("assets/models/leeperrysmith/lps.obj");
            Geometry combined;
            for (int i = 0; i < leePerryHeadModel.size(); ++i)
            {
                auto & m = leePerryHeadModel[i];
                for (auto & v : m.vertices) v *= 15.f;
                combined = concatenate_geometry(combined, m);
            }
            combined.compute_normals(false);
            proceduralModels[3] = Renderable(combined);
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
                decalModels.clear();
            }
        }
        
        if (event.type == InputEvent::MOUSE && event.action == GLFW_PRESS)
        {
            if (event.value[0] == GLFW_MOUSE_BUTTON_LEFT)
            {
                for (auto & model : proceduralModels)
                {
                    auto worldRay = camera.get_world_ray(event.cursor, float2(event.windowSize));
                    auto hit = model.check_hit(worldRay);
                    if (std::get<0>(hit))
                    {
                        float3 position = worldRay.calculate_position(std::get<1>(hit));
                        float3 target = (std::get<2>(hit) * float3(10, 10, 10)) + position;
                        
                        Pose box(position);
                        look_at_pose(position, target, box);
                        
                        decalModels.push_back(Renderable(make_decal_geometry(model, box, float3(0.5f))));
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

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        
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
            
            simpleShader->uniform("u_eye", camera.get_eye_point());
            simpleShader->uniform("u_viewProj", viewProj);
            
            simpleShader->uniform("u_emissive", float3(.10f, 0.10f, 0.10f));
            simpleShader->uniform("u_diffuse", float3(0.5f, 0.4f, 0.4f));
            simpleShader->uniform("useNormal", 0);
            
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
                simpleShader->texture("u_diffuseTex", 0, emptyTex);
                model.draw();
            }

            {
                glEnable (GL_POLYGON_OFFSET_FILL);
                glPolygonOffset (-1.0, 1.0);
                
                for (const auto & decal : decalModels)
                {
                    simpleShader->uniform("u_modelMatrix", decal.get_model());
                    simpleShader->uniform("u_modelMatrixIT", inv(transpose(decal.get_model())));
                    simpleShader->texture("u_diffuseTex", 0, anvilTex);
                    decal.draw();
                }
                
                glDisable(GL_POLYGON_OFFSET_FILL);
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
