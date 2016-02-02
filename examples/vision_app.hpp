#include "index.hpp"

#include "librealsense/rs.hpp"

struct ExperimentalApp : public GLFWApp
{
    uint64_t frameCount = 0;

    GlCamera camera;

    GlTexture depthTexture;
    GlTexture normalTexture;
    
    std::unique_ptr<GLTextureView> depthTextureView;
    std::unique_ptr<GLTextureView> normalTextureView;
   
    UIComponent uiSurface;
    
    rs::context ctx;
    rs::device * dev;
    
    bool streaming = false;
    int cameraWidth = 0;
    int cameraHeight = 0;
    
    struct CameraFrame
    {
        std::vector<uint16_t> depthmap;
        std::vector<float3> normalmap;
        std::vector<uint8_t> rgbDepth;
        std::vector<uint8_t> rgbNormal;
    };
    
    CameraFrame f;
    
    ExperimentalApp() : GLFWApp(1280, 720, "Vision App")
    {
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
        
        try
        {
            std::cout << "There are " << ctx.get_device_count() <<  "connected RealSense devices.\n";
            if (ctx.get_device_count() == 0) throw std::runtime_error("No cameras plugged in?");
            
            dev = ctx.get_device(0);
            std::cout << "Using " << dev->get_name() << std::endl;
            std::cout << "Serial: " << dev->get_serial() << std::endl;
            std::cout << "Firmware: " << dev->get_firmware_version() << std::endl;
            
            dev->enable_stream(rs::stream::depth, 320, 240, rs::format::z16, 60);
            dev->enable_stream(rs::stream::color, 640, 480, rs::format::rgb8, 60);
            
            auto intrin = dev->get_stream_intrinsics(rs::stream::depth);
            cameraWidth = intrin.width; cameraHeight = intrin.height;
            
            f.depthmap.resize(intrin.width * intrin.height);
            f.normalmap.resize(intrin.width * intrin.height);
            f.rgbDepth.resize(3 * intrin.width * intrin.height);
            f.rgbNormal.resize(3 * intrin.width * intrin.height);
            
            dev->start();
            
            streaming = true;
        }
        catch(const rs::error & e)
        {
            printf("rs::error was thrown when calling %s(%s):\n", e.get_failed_function().c_str(), e.get_failed_args().c_str());
            printf("    %s\n", e.what());
        }
    
        uiSurface.bounds = {0, 0, (float) width, (float) height};
        uiSurface.add_child( {{0,+10},{0,+10},{0.5,0},{0.5,0}}, std::make_shared<UIComponent>());
        uiSurface.add_child( {{.50,+10},{0, +10},{1.0, -10},{0.5,0}}, std::make_shared<UIComponent>());
        uiSurface.layout();
        
        // Generate texture handles
        depthTexture.load_data(cameraWidth, cameraHeight, GL_RGB, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
        normalTexture.load_data(cameraWidth, cameraHeight, GL_RGB, GL_RGB, GL_FLOAT, nullptr);
        
        depthTextureView.reset(new GLTextureView(depthTexture.get_gl_handle()));
        normalTextureView.reset(new GLTextureView(normalTexture.get_gl_handle()));
        
        gl_check_error(__FILE__, __LINE__);
        camera.look_at({0, 2.5, -2.5}, {0, 2.0, 0});
    }

    void on_window_resize(int2 size) override
    {
        uiSurface.bounds = {0, 0, (float) size.x, (float) size.y};
        uiSurface.layout();
    }
    
    void on_input(const InputEvent & event) override
    {

    }
    
    void on_update(const UpdateEvent & e) override
    {
        
        if (streaming)
        {
            dev->wait_for_frames();
            
            const uint16_t * depth_frame = reinterpret_cast<const uint16_t *>(dev->get_frame_data(rs::stream::depth));
            auto intrin = dev->get_stream_intrinsics(rs::stream::depth);
            
            avl_intrin i = {intrin.width, intrin.height, intrin.ppx, intrin.ppy, intrin.fx, intrin.fy};
        
            std::memset(f.depthmap.data(), 0, sizeof(uint16_t) * intrin.width * intrin.height);
            std::memset(f.normalmap.data(), 0, sizeof(float3) * intrin.width * intrin.height);
            std::memset(f.rgbDepth.data(), 0, 3 * intrin.width * intrin.height);
            std::memset(f.rgbNormal.data(), 0, 3 * intrin.width * intrin.height);
        
            std::memcpy(f.depthmap.data(), depth_frame, sizeof(uint16_t) * intrin.width * intrin.height);
            
            generate_normalmap<1>(f.depthmap, f.normalmap, i);
            
            depth_to_colored_histogram(f.rgbDepth, f.depthmap, float2(intrin.width, intrin.height), float2(0.125f, 0.45f));
            
            for (int s = 0; s < intrin.width * intrin.height; ++s)
            {
                const float3 & normal = f.normalmap[s];
                f.rgbNormal[3 * s + 0] = static_cast<uint8_t>(128 + normal.x * 127);
                f.rgbNormal[3 * s + 1] = static_cast<uint8_t>(128 + normal.y * 127);
                f.rgbNormal[3 * s + 2] = static_cast<uint8_t>(128 + normal.z * 127);
            }
            
            depthTexture.load_data(cameraWidth, cameraHeight, GL_RGB, GL_RGB, GL_UNSIGNED_BYTE, f.rgbDepth.data());
            normalTexture.load_data(cameraWidth, cameraHeight, GL_RGB, GL_RGB, GL_UNSIGNED_BYTE, f.rgbNormal.data());
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

        //const float4x4 proj = camera.get_projection_matrix((float) width / (float) height);
        //const float4x4 view = camera.get_view_matrix();
        //const float4x4 viewProj = mul(proj, view);

        depthTextureView->draw(uiSurface.children[0]->bounds, int2(width, height));
        normalTextureView->draw(uiSurface.children[1]->bounds, int2(width, height));
        
        gl_check_error(__FILE__, __LINE__);
        
        glfwSwapBuffers(window);
        
        frameCount++;
    }
    
};
