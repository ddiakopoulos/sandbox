#include "index.hpp"

// http://blog.wolfire.com/2009/06/how-to-project-decals/

enum DecalProjectionType
{
    PROJECTION_TYPE_CAMERA,
    PROJECTION_TYPE_NORMAL
};

struct DecalVertex
{
    float3 v, n;
    DecalVertex(float3 v, float3 n) : v(v), n(n) {}
    DecalVertex() {}
};

inline std::vector<DecalVertex> clip_face(const std::vector<DecalVertex> & inVertices, float3 dimensions, float3 plane)
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
        
        // How many verts are on this side of the plane?
        int total = v1Out + v2Out + v3Out;

        DecalVertex nV1, nV2, nV3, nV4;
        
        switch (total)
        {
                
            // None - don't clip the geometry
            case 0:
            {
                outVertices.push_back(inVertices[j + 0]);
                outVertices.push_back(inVertices[j + 1]);
                outVertices.push_back(inVertices[j + 2]);
                break;
            }
                
            // One vert
            case 1:
            {
                
                // v1 has been marked
                if (v1Out)
                {
                    nV1 = inVertices[j + 1];
                    nV2 = inVertices[j + 2];
                    nV3 = clip(inVertices[j + 0], nV1, plane);
                    nV4 = clip(inVertices[j + 0], nV2, plane);
                    
                    outVertices.push_back(nV1);
                    outVertices.push_back(nV2);
                    outVertices.push_back(nV3);
                    
                    outVertices.push_back(nV4);
                    outVertices.push_back(nV3);
                    outVertices.push_back(nV2);
                }
                
                // v1 has been marked
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
                }
                
                // v3 has been marked
                if (v3Out)
                {
                    nV1 = inVertices[j + 0];
                    nV2 = inVertices[j + 1];
                    nV3 = clip(inVertices[j + 2], nV1, plane);
                    nV4 = clip(inVertices[j + 2], nV2, plane);

                    outVertices.push_back(nV1);
                    outVertices.push_back(nV2);
                    outVertices.push_back(nV3);
                    
                    outVertices.push_back(nV4);
                    outVertices.push_back(nV3);
                    outVertices.push_back(nV2);
                }
                
                break;
            }
                
            // Two verts
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
                    nV3 = clip(nV1, inVertices[j + 0], plane);
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
                
            // All outside
            case 3:
            {
                break;
            }
        }
        
    }
    
    return outVertices;
}

inline Geometry make_decal_geometry(SimpleStaticMesh & r, const Pose & cubePose, const float3 & dimensions)
{
    Geometry decal;
    std::vector<DecalVertex> finalVertices;
    
    auto & mesh = r.get_geometry();
    auto & pose = r.get_pose();

    assert(mesh.normals.size() > 0);
    
    for (int i = 0; i < mesh.faces.size(); i++)
    {
        uint3 f = mesh.faces[i];
        std::vector<DecalVertex> clippedVertices;
        
        for (int j = 0; j < 3; j++)
        {
            float3 v = mesh.vertices[f[j]];
            float3 n = mesh.normals[f[j]];
            v = transform_coord(pose.matrix(), v); // local into world
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
        
        // Projected coordinates are the texture coordinates
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
    FlyCameraController cameraController;
    
    std::vector<SimpleStaticMesh> proceduralModels;
    std::vector<SimpleStaticMesh> decalModels;
   
    std::unique_ptr<GlShader> simpleShader;
    
    GlTexture2D anvilTex;
    GlTexture2D emptyTex;
    
    DecalProjectionType projType = PROJECTION_TYPE_CAMERA;
    
    ExperimentalApp() : GLFWApp(1280, 720, "Decal App")
    {
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
        
        cameraController.set_camera(&camera);
        
        camera.look_at({0, 8, 15}, {0, 0.1f, 0});

        simpleShader.reset(new GlShader(read_file_text("../assets/shaders/textured_model_vert.glsl"), read_file_text("../assets/shaders/textured_model_frag.glsl")));
        
        anvilTex = load_image("../assets/images/polygon_heart.png");
        
        std::vector<uint8_t> pixel = {255, 255, 255, 255};
        emptyTex.setup(1, 1, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());

        {
            proceduralModels.resize(3);

            proceduralModels[0].set_static_mesh(make_torus());
            proceduralModels[0].set_pose(Pose(float3(0, 2, +8)));

            proceduralModels[1].set_static_mesh(make_cube());
            proceduralModels[1].set_pose(Pose(float3(0, 2, -8)));

            auto leePerryHeadModel = load_geometry_from_obj_no_texture("../assets/models/leeperrysmith/lps.obj");
            Geometry combined;
            for (int i = 0; i < leePerryHeadModel.size(); ++i)
            {
                auto & m = leePerryHeadModel[i];
                for (auto & v : m.vertices) v *= 15.f;
                combined = concatenate_geometry(combined, m);
            }
            combined.compute_normals(false);

            proceduralModels[2].set_static_mesh(combined);
            proceduralModels[2].set_pose(Pose(float3(-8, 2, 0)));
        }
        
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
            if (event.value[0] == GLFW_KEY_SPACE && event.action == GLFW_RELEASE) decalModels.clear();
            if (event.value[0] == GLFW_KEY_1 && event.action == GLFW_RELEASE) projType = PROJECTION_TYPE_CAMERA;
            if (event.value[0] == GLFW_KEY_2 && event.action == GLFW_RELEASE) projType = PROJECTION_TYPE_NORMAL;
        }
        
        if (event.type == InputEvent::MOUSE && event.action == GLFW_PRESS)
        {
            if (event.value[0] == GLFW_MOUSE_BUTTON_LEFT)
            {
                for (auto & model : proceduralModels)
                {
                    auto worldRay = camera.get_world_ray(event.cursor, float2(event.windowSize));
                    
                    RaycastResult rc = model.raycast(worldRay);
                    
                    if (rc.hit)
                    {
                        float3 position = worldRay.calculate_position(rc.distance);
                        float3 target = (rc.normal * float3(10, 10, 10)) + position;
                        
                        Pose box;

                        // Option A: Camera to mesh (orientation artifacts, better uv projection across hard surfaces)
                        if (projType == PROJECTION_TYPE_CAMERA)
                        {
                            box = Pose(camera.get_pose().orientation, position);
                        }

                        // Option B: Normal to mesh (uv issues)
                        else if (projType == PROJECTION_TYPE_NORMAL)
                        {
                            box = look_at_pose_rh(position, target);
                        }

                        SimpleStaticMesh m;
                        Geometry newDecalGeometry = make_decal_geometry(model, box, float3(0.5f));
                        m.set_static_mesh(newDecalGeometry);

                        decalModels.push_back(std::move(m));
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
        glClearColor(0.80f, 0.80f, 0.80f, 1.0f);

        const float4x4 projectionMatrix = camera.get_projection_matrix((float)width / (float)height);
        const float4x4 viewMatrix = camera.get_view_matrix();
        const float4x4 viewProjectionMatrix = mul(projectionMatrix, viewMatrix);
        
        // Simple Shader
        {
            simpleShader->bind();
            
            simpleShader->uniform("u_eyePos", camera.get_eye_point());
            simpleShader->uniform("u_viewProjMatrix", viewProjectionMatrix);
            simpleShader->uniform("u_viewMatrix", viewMatrix);

            simpleShader->uniform("u_ambientLight", float3(0.5f));

            simpleShader->uniform("u_rimLight.enable", 0);

            simpleShader->uniform("u_material.diffuseIntensity", float3(1.0f, 1.0f, 1.0f));
            simpleShader->uniform("u_material.ambientIntensity", float3(1.0f, 1.0f, 1.0f));
            simpleShader->uniform("u_material.specularIntensity", float3(1.0f, 1.0f, 1.0f));
            simpleShader->uniform("u_material.specularPower", 8.0f);

            simpleShader->uniform("u_lights[0].position", float3(10, 12, 0));
            simpleShader->uniform("u_lights[0].color", float3(249.f / 255.f, 228.f / 255.f, 157.f / 255.f));
            simpleShader->uniform("u_lights[1].position", float3(0, 0, 0));
            simpleShader->uniform("u_lights[1].color", float3(255.f / 255.f, 242.f / 255.f, 254.f / 255.f));

            for (const auto & model : proceduralModels)
            {
                simpleShader->uniform("u_modelMatrix", model.get_pose().matrix());
                simpleShader->uniform("u_modelMatrixIT", inv(transpose(model.get_pose().matrix())));
                simpleShader->texture("u_diffuseTex", 0, emptyTex, GL_TEXTURE_2D);
                model.draw();
            }

            {
                glEnable(GL_POLYGON_OFFSET_FILL);
                glPolygonOffset (-1.0, 1.0);
                
                for (const auto & decal : decalModels)
                {
                    simpleShader->uniform("u_modelMatrix", decal.get_pose().matrix());
                    simpleShader->uniform("u_modelMatrixIT", inv(transpose(decal.get_pose().matrix())));
                    simpleShader->texture("u_diffuseTex", 0, anvilTex, GL_TEXTURE_2D);
                    decal.draw();
                }
                
                glDisable(GL_POLYGON_OFFSET_FILL);
            }
            
            simpleShader->unbind();
        }

        glfwSwapBuffers(window);
        
        frameCount++;
    }
    
};
