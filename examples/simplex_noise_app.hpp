// Adapted from https://github.com/simongeilfus/SimplexNoise/blob/master/samples/NoiseGallery/src/NoiseGalleryApp.cpp

#include "index.hpp"

using namespace noise;

struct ExperimentalApp : public GLFWApp
{
    Space uiSurface;
    
    std::vector<std::shared_ptr<GlTexture>> textures;
    std::vector<std::shared_ptr<GLTextureView>> textureViews;

    const int texResolution = 128;
    std::vector<uint8_t> data;

    std::random_device rd;
    std::mt19937 gen;

    ExperimentalApp() : GLFWApp(1024, 1024, "Simplex Noise App")
    {
        gen = std::mt19937(rd());
        
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
        
        uiSurface.bounds = {0, 0, (float) width, (float) height};
        
        data.resize(texResolution * texResolution);
        
        float sz = 1.0f / 4.f;
        for (int i = 0; i < 4; ++i)
        {
            for (int j = 0; j < 4; ++j)
            {
                uiSurface.add_child( {{j * sz,0},{i * sz,0},{(j + 1) * sz, 0}, {(i + 1) * sz, 0}}, std::make_shared<Space>());
            }
        }
        uiSurface.layout();
        
        for (int i = 0; i < 16; i++)
        {
            auto t = std::make_shared<GlTexture>();
            t->load_data(texResolution, texResolution, GL_RED, GL_RED, GL_UNSIGNED_BYTE, nullptr);
            
            auto tv = std::make_shared<GLTextureView>(t->get_gl_handle());
            
            textures.push_back(t);
            textureViews.push_back(tv);
        }

        gl_check_error(__FILE__, __LINE__);
    }

    void on_window_resize(int2 size) override
    {
        uiSurface.bounds = {0, 0, (float) size.x, (float) size.y};
        uiSurface.layout();
    }
    
    void on_input(const InputEvent & event) override
    {
        if (event.type == InputEvent::KEY)
        {
            if (event.value[0] == GLFW_KEY_SPACE && event.action == GLFW_RELEASE)
            {
                noise::regenerate_permutation_table(gen);
            }
        }
    }
    
    void on_update(const UpdateEvent & e) override
    {
        const float time = e.elapsed_s;
        
        for (int i = 0; i < 16; ++i)
        {
            if (e.elapsedFrames > 1)
            {
                switch (i)
                {
                    case 2: case 7: case 8: case 15: break;
                    default: continue; break;
                }
            }
            
            for (int x = 0; x < texResolution; ++x)
            {
                for (int y = 0; y < texResolution; ++y)
                {
                    float n = 0.0f;
                    auto position = float2(x, y) * 0.01f;
                    switch (i)
                    {
                        case 0: n = noise::noise(position) * 0.5f + 0.5f; break;
                        case 1: n = noise_ridged(position); break;
                        case 2: n = noise_flow(position, time) * 0.5f + 0.5f; break;
                        case 3: n = noise_fb(position) * 0.5f + 0.5f; break;
                        case 4: n = noise_fb(position, 10, 5.0f, 0.75f) * 0.5f + 0.5f; break;
                        case 5: n = noise_fb(noise_fb(position * 3.0f)) * 0.5f + 0.5f; break;
                        case 6: n = noise_fb(noise_fb_deriv(position)) * 0.5f + 0.5f; break;
                        case 7: n = noise_flow(position + noise_fb(float3(position, time * 0.1f)), time) * 0.5f + 0.5f; break;
                        case 8: n = noise_ridged_mf(float3(position, time * 0.1f), 1.0f, 5, 2.0f, 0.65f); break;
                        case 9: n = noise_ridged_mf(position, 0.1f, 5, 1.5f, 1.5f); break;
                        case 10: n = noise_ridged_mf(noise_ridged(position)); break;
                        case 11: n = noise_ridged_mf(position * 0.25f, -1.0f, 4, 3.0f, -0.65f); break;
                        case 12: n = noise_iq_fb(position, 5, float2x2({2.3f, -1.5f}, {1.5f, 2.3f}), 0.5f) * 0.5f + 0.5f; break;
                        case 13: n = noise_iq_fb(position * 0.75f, 8, float2x2({-12.5f, -0.5f}, {0.5f, -12.5f}), 0.75f) * 0.5f + 0.5f; break;
                        case 14: n = (noise_deriv(position * 5.0f).y + noise_deriv(position * 5.0f).z) * 0.5f; break;
                        case 15: n = noise::noise(position + float2(noise_curl(position, time).x)) * 0.5f + 0.5f; break;
                            
                    }
                    data[y * texResolution + x] = clamp(n, 0.0f, 1.0f) * 255;
                }
            }
            
            textures[i]->load_data(texResolution, texResolution, GL_RED, GL_RED, GL_UNSIGNED_BYTE, data.data());
        }
        
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

        for (int i = 0; i < 16; i++)
        {
           textureViews[i]->draw(uiSurface.children[i]->bounds, int2(width, height));
        }
        
        gl_check_error(__FILE__, __LINE__);
        
        glfwSwapBuffers(window);
    }
    
};
