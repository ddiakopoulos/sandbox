#include "index.hpp"
#include "../third_party/jo_gif.hpp"

using namespace math;
using namespace util;
using namespace gfx;

std::vector<bool> make_euclidean_rhythm(int steps, int pulses)
{
    std::vector<bool> pattern;
    
    std::function<void(int, int, std::vector<bool> &, std::vector<int> &, std::vector<int> &)> bjorklund;
    
    bjorklund = [&bjorklund](int level, int r, std::vector<bool> & pattern, std::vector<int> & counts, std::vector<int> & remainders)
    {
        r++;
        if (level > -1)
        {
            for (int i=0; i < counts[level]; ++i)
                bjorklund(level - 1, r, pattern, counts, remainders);
            if (remainders[level] != 0)
                bjorklund(level - 2, r, pattern, counts, remainders);
        }
        else if (level == -1) pattern.push_back(false);
        else if (level == -2) pattern.push_back(true);
    };
    
    if (pulses > steps || pulses == 0 || steps == 0)
        return pattern;
    
    std::vector<int> counts;
    std::vector<int> remainders;
    
    int divisor = steps - pulses;
    remainders.push_back(pulses);
    int level = 0;
    
    while (true)
    {
        counts.push_back(divisor / remainders[level]);
        remainders.push_back(divisor % remainders[level]);
        divisor = remainders[level];
        level++;
        if (remainders[level] <= 1) break;
    }
    
    counts.push_back(divisor);
    
    bjorklund(level, 0, pattern, counts, remainders);
    
    return pattern;
}

// gif = jo_gif_start("euclidean.gif", width, height, 0, 255);
// glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, rgbFrame.data());
// flip_image(rgbFrame.data(), width, height, 4);
// jo_gif_frame(&gif, rgbFrame.data(), 12, false);
// jo_gif_end(&gif);
// std::vector<unsigned char> rgbFrame;
// rgbFrame.resize(width * height * 4);

static const float TEXT_OFFSET_X = 3;
static const float TEXT_OFFSET_Y = 1;

struct UIState
{
    NVGcontext * nvg;
    float2 cursor;
    bool leftBtn;
    bool rightBtn;
    std::shared_ptr<NvgFont> typeface;
};

struct PanelControl : public UWidget
{
    void render(std::shared_ptr<UIState> state)
    {
        auto ctx = state->nvg;
        nvgBeginPath(ctx);
        nvgRect(ctx, bounds.x0, bounds.y0, bounds.width(), bounds.height());
        nvgStrokeColor(ctx, nvgRGBA(255, 255, 255, 255));
        nvgStrokeWidth(ctx, 1.0f);
        nvgStroke(ctx);
    }
};

struct LabelControl : public UWidget
{
    float render(std::shared_ptr<UIState> state, const std::string & text)
    {
        auto ctx = state->nvg;
        const float textX = bounds.x0 + TEXT_OFFSET_X, textY = bounds.y0 + TEXT_OFFSET_Y;
        nvgFontFaceId(ctx, state->typeface->id);
        nvgFontSize(ctx, 20);
        nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
        nvgBeginPath(ctx);
        nvgFillColor(ctx, nvgRGBA(0,0,0,255));
        return nvgText(ctx, textX, textY, text.c_str(), nullptr);
    }
};

struct ButtonControl : public UWidget
{
    bool render(std::shared_ptr<UIState> state, const std::string & text)
    {
        auto ctx = state->nvg;
        
        const float textX = bounds.x0 + TEXT_OFFSET_X, textY = bounds.y0 + TEXT_OFFSET_Y;
        nvgFontFaceId(ctx, state->typeface->id);
        nvgFontSize(ctx, 20);
        nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
        nvgBeginPath(ctx);
        nvgFillColor(ctx, nvgRGBA(0,0,0,255));
        nvgText(ctx, textX, textY, text.c_str(), nullptr);
        
        if (bounds.inside(state->cursor) && state->leftBtn == true)
        {
            nvgBeginPath(ctx);
            nvgRect(ctx, bounds.x0, bounds.y0, bounds.width(), bounds.height());
            nvgStrokeColor(ctx, nvgRGBA(255, 0, 0, 255));
            nvgStrokeWidth(ctx, 1.0f);
            nvgStroke(ctx);
            return true;
        }
        
        nvgBeginPath(ctx);
        nvgRect(ctx, bounds.x0, bounds.y0, bounds.width(), bounds.height());
        nvgStrokeColor(ctx, nvgRGBA(255, 255, 255, 255));
        nvgStrokeWidth(ctx, 1.0f);
        nvgStroke(ctx);
        return false;
    };
};

struct SliderControl : public UWidget
{
    bool render(std::shared_ptr<UIState> state, const std::string & text, const float min, const float max, float & value)
    {
        if (bounds.inside(state->cursor) && state->leftBtn == true)
        {
            return true;
        }
        return false;
    };
};

struct ExperimentalApp : public GLFWApp
{
    uint64_t frameCount = 0;
    
    GlCamera camera;
    HosekProceduralSky skydome;
    RenderableGrid grid;
    FPSCameraController cameraController;
    
    std::vector<Renderable> proceduralModels;
    std::vector<Renderable> cameraPositions;
    std::vector<LightObject> lights;
    
    std::unique_ptr<GlShader> simpleShader;
    
    std::vector<bool> euclideanPattern;
    
    float rotationAngle = 0.0f;
    
    jo_gif_t gif;
    
    NVGcontext * context;
    UWidget uiRootNode;
    
    std::shared_ptr<NvgFont> sourceFont;
    std::shared_ptr<UIState> uiState;
    
    std::shared_ptr<LabelControl> label;
    std::shared_ptr<ButtonControl> button;
    
    ExperimentalApp() : GLFWApp(940, 720, "Euclidean App")
    {
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
        
        cameraController.set_camera(&camera);
        
        camera.look_at({0, 8, 24}, {0, 0, 0});
        
        simpleShader.reset(new gfx::GlShader(read_file_text("assets/shaders/simple_vert.glsl"), read_file_text("assets/shaders/simple_frag.glsl")));
        
        {
            lights.resize(2);
            lights[0].color = float3(249.f / 255.f, 228.f / 255.f, 157.f / 255.f);
            lights[0].pose.position = float3(25, 15, 0);
            lights[1].color = float3(255.f / 255.f, 242.f / 255.f, 254.f / 255.f);
            lights[1].pose.position = float3(-25, 15, 0);
        }
        
        euclideanPattern = make_euclidean_rhythm(16, 4);
        std::rotate(euclideanPattern.rbegin(), euclideanPattern.rbegin() + 1, euclideanPattern.rend()); // Rotate right
        std::cout << "Pattern Size: " << euclideanPattern.size() << std::endl;
        
        for (int i = 0; i < euclideanPattern.size(); i++)
        {
            proceduralModels.push_back(Renderable(make_icosahedron()));
        }
        
        float r = 16.0f;
        float thetaIdx = ANVIL_TAU / proceduralModels.size();
        auto offset = 0;
        
        for (int t = 1; t < proceduralModels.size() + 1; t++)
        {
            auto & obj = proceduralModels[t - 1];
            obj.pose.position = { float(r * sin((t * thetaIdx) - offset)), 4.0f, float(r * cos((t * thetaIdx) - offset))};
        }
        
        grid = RenderableGrid(1, 64, 64);
        
        {
            context = setup_user_interface();
            uiState.reset(new UIState);
            uiState->nvg = context;
            uiState->typeface = sourceFont;
            
            uiRootNode.bounds = {0, 0, (float) width, (float) height};
            
            label = std::make_shared<LabelControl>();
            button = std::make_shared<ButtonControl>();
            
            uiRootNode.add_child( {{0,+10},{0,+10},{0.25,0},{0.25,0}}, label);
            uiRootNode.add_child( {{.25,+10},{0, +10},{0.50, -10},{0.25,0}}, button);
                
            uiRootNode.layout();
        }
        
        gfx::gl_check_error(__FILE__, __LINE__);
    }
    
    NVGcontext * setup_user_interface()
    {
        NVGcontext * nvgCtx = make_nanovg_context(NVG_ANTIALIAS | NVG_STENCIL_STROKES);
        if (!nvgCtx) throw std::runtime_error("error initializing nanovg context");
        sourceFont = std::make_shared<NvgFont>(nvgCtx, "souce_sans_pro", read_file_binary("assets/fonts/source_code_pro_regular.ttf"));
        return nvgCtx;
    }
    
    void draw_user_interface()
    {
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        
        nvgBeginFrame(context, width, height, 1.0);
        {
            button->render(uiState, "Test Button");
            label->render(uiState, "Some label...");
        }
        nvgEndFrame(context);
    }
    
    ~ExperimentalApp()
    {

    }
    
    void on_window_resize(math::int2 size) override
    {
        
    }
    
    void on_input(const InputEvent & event) override
    {
        cameraController.handle_input(event);
    }
    
    void on_update(const UpdateEvent & e) override
    {
        cameraController.update(e.timestep_ms);
        rotationAngle += e.timestep_ms;
        
        for (int i = 0; i < euclideanPattern.size(); ++i)
        {
            auto value = euclideanPattern[i];
            if (value) proceduralModels[i].pose.orientation = make_rotation_quat_axis_angle({0, 1, 0}, 0.88f * rotationAngle);
        }
    }
    
    void on_draw() override
    {
        glfwMakeContextCurrent(window);
        
        glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_DEPTH_TEST);
        
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
        
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glClearColor(0.1f, 0.1f, 0.5f, 1.0f);
        
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
            
            int patternIdx = 0;
            for (const auto & model : proceduralModels)
            {
                bool pulse = euclideanPattern[patternIdx];
                simpleShader->uniform("u_modelMatrix", model.get_model());
                simpleShader->uniform("u_modelMatrixIT", inv(transpose(model.get_model())));
                if (pulse) simpleShader->uniform("u_diffuse", float3(0.7f, 0.3f, 0.3f));
                else  simpleShader->uniform("u_diffuse", float3(0.4f, 0.4f, 0.4f));
                model.draw();
                patternIdx++;
            }
            
            gfx::gl_check_error(__FILE__, __LINE__);
            
            simpleShader->unbind();
        }
        
        grid.render(proj, view);
        
        draw_user_interface();
        
        gfx::gl_check_error(__FILE__, __LINE__);
        
        glfwSwapBuffers(window);
        
        frameCount++;
    }
    
};
