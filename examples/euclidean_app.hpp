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

// [UI Todo]
// * Button interaction
// * Slider interaction
// * Resize interaction

static const float TEXT_OFFSET_X = 3;
static const float TEXT_OFFSET_Y = 1;

struct PanelControl : public UIComponent
{
    PanelControl(UIStyleSheet ss) : UIComponent(ss) {};
    
    virtual void render(const UIRenderEvent & e) override
    {
        auto ctx = e.ctx;
        
        // Border
        nvgBeginPath(ctx);
        nvgRect(ctx, bounds.x0, bounds.y0, bounds.width(), bounds.height());
        nvgStrokeColor(ctx, style.borderColor);
        nvgStrokeWidth(ctx, 1.0f);
        nvgStroke(ctx);
        
        // Handle
        nvgBeginPath(ctx);
        nvgRect(ctx, bounds.x0 + 1, bounds.y0 + 1, bounds.width() - 1, bounds.height() - 1);
        nvgFillColor(ctx, style.backgroundColor);
        nvgFill(ctx);
    };
};


struct LabelControl : public UIComponent
{
    std::string text;
    void set_text(const std::string & t) { text = t; };
    
    LabelControl(UIStyleSheet ss) : UIComponent(ss) {};
    
    virtual void render(const UIRenderEvent & e) override
    {
        auto ctx = e.ctx;
        float w = nvgTextBounds(ctx, 0, 0, text.c_str(), NULL, NULL);
        const float textX = bounds.get_center_x() - w * 0.5f + 3, textY = bounds.get_center_y() + 1;
        nvgFontFaceId(ctx, e.text->id);
        nvgFontSize(ctx, 20);
        nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        nvgBeginPath(ctx);
        nvgFillColor(ctx, style.textColor);
        nvgText(ctx, textX, textY, text.c_str(), nullptr);
    }
    
    virtual void on_mouse_down(const float2 cursor) override { std::cout << "Label Click: " << cursor << std::endl; }
    
};

struct ButtonControl : public UIComponent
{
    bool * value;
    
    bool hover = false;
    
    std::string text;
    void set_text(const std::string & t) { text = t; };
    void set_variable(bool & v) { value = &v; }
    
    ButtonControl(UIStyleSheet ss) : UIComponent(ss) {};
    
    virtual void render(const UIRenderEvent & e) override
    {
        auto ctx = e.ctx;
        float w = nvgTextBounds(ctx, 0, 0, text.c_str(), NULL, NULL);
        const float textX = bounds.get_center_x() - w * 0.5f + 3, textY = bounds.get_center_y() + 1;
        nvgFontFaceId(ctx, e.text->id);
        nvgFontSize(ctx, 20);
        nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        nvgBeginPath(ctx);
        nvgFillColor(ctx, style.textColor);
        nvgText(ctx, textX, textY, text.c_str(), nullptr);
        
        if (hover)
        {
            nvgBeginPath(ctx);
            nvgRect(ctx, bounds.x0, bounds.y0, bounds.width(), bounds.height());
            nvgStrokeColor(ctx, style.borderColor);
            nvgStrokeWidth(ctx, 1.0f);
            nvgStroke(ctx);
        }
        
        nvgBeginPath(ctx);
        nvgRect(ctx, bounds.x0, bounds.y0, bounds.width(), bounds.height());
        nvgStrokeColor(ctx, style.borderColor);
        nvgStrokeWidth(ctx, 1.0f);
        nvgStroke(ctx);
    };
    
    virtual void on_mouse_down(const float2 cursor) override { std::cout << "Button Click: " << cursor << std::endl; }
    virtual void on_mouse_up(const float2 cursor) override { std::cout << "Button Release: " << cursor << std::endl; }
    
};

struct SliderControl : public UIComponent
{
    float min, max;
    float * value;
    float handleOffset;
    float2 lastClick;
    const float handleSize = 20.0f; //pixels
    std::string text;
    
    void set_text(const std::string & t) { text = t; };
    void set_range (const float min, const float max) { this->min = min; this->max = max; }
    void set_variable(float & v) { value = &v; }
    
    SliderControl(UIStyleSheet ss) : UIComponent(ss) {};
    
    virtual void render(const UIRenderEvent & e) override
    {
        auto ctx = e.ctx;
        
        // Border
        nvgBeginPath(ctx);
        nvgRect(ctx, bounds.x0, bounds.y0, bounds.width(), bounds.height());
        nvgStrokeColor(ctx, style.borderColor);
        nvgStrokeWidth(ctx, 1.0f);
        nvgStroke(ctx);

        // Left-justified label
        {
            const float textX = bounds.get_min().x + 3, textY = bounds.get_center_y() + 1;
            nvgFontFaceId(ctx, e.text->id);
            nvgFontSize(ctx, 20);
            nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
            nvgBeginPath(ctx);
            nvgFillColor(ctx, style.textColor);
            nvgText(ctx, textX, textY, text.c_str(), nullptr);
        }
        
        // Right-justified value
        {
            float w = nvgTextBounds(ctx, 0, 0, text.c_str(), NULL, NULL);
            const float textX = bounds.get_max().x - w + 3, textY = bounds.get_center_y() + 1;
            nvgFontFaceId(ctx, e.text->id);
            nvgFontSize(ctx, 20);
            nvgTextAlign(ctx, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
            nvgBeginPath(ctx);
            nvgFillColor(ctx, style.textColor);
            nvgText(ctx, textX, textY, std::to_string(*value).c_str(), nullptr);
        }
        
        // Handle
        nvgBeginPath(ctx);
        nvgRect(ctx, bounds.x0 + handleOffset, bounds.y0, handleSize, bounds.height());
        nvgFillColor(ctx, nvgRGBA(255, 0, 0, 255));
        nvgFill(ctx);
    };
    
    void compute_offset(const float2 cursor)
    {
        float2 offsetFromx0 = lastClick - bounds.x0;
        handleOffset = clamp(offsetFromx0.x - (handleSize * 0.5f) - (lastClick.x - cursor.x), 0.0f, bounds.width() - (handleSize));
        *value = remap<float>(handleOffset, 0.0f, bounds.width() - (handleSize), min, max, true);
    }
    
    virtual void on_mouse_down(const float2 cursor) override { lastClick = cursor; compute_offset(cursor); }
    
    virtual void on_mouse_drag(const math::float2 cursor, const math::float2 delta) override { compute_offset(cursor); }
};

// A UISurface creates and owns a nanovg context and related font assets. The root
// node covers the surface area of the window and can be partitioned by children.
// The surfaces handles input events from the application and redraws the tree.
class UISurface
{
    NVGcontext * nvg;
    
    std::shared_ptr<NvgFont> text_fontface;
    std::shared_ptr<NvgFont> icon_fontface;
    std::shared_ptr<UIComponent> root;
    
    UIStyleSheet stylesheet;
    
    std::shared_ptr<UIComponent> hoverNode;
    std::shared_ptr<UIComponent> clickedNode;
    float2 lastCursor;
    
    std::shared_ptr<UIComponent> get_hover_component(const std::shared_ptr<UIComponent> & component, const float2 cursor)
    {
        bool hit = component->bounds.inside(cursor);
        if (!hit) return nullptr;
        for (auto it = component->children.crbegin(), end = component->children.crend(); it != end; ++it)
            if (auto result = get_hover_component(*it, cursor)) return result;
        return component;
    }
    
    void render(UIRenderEvent & e, std::shared_ptr<UIComponent> & control)
    {
        // Draw current and recurse into children
        control->render(e);
        for (auto & c : control->children)
        {
            e.parent = control.get();
            render(e, c);
        }
    }
    
public:
    
    UISurface(float width, float height, const std::string & text_font, const std::string & icon_font)
    {
        nvg = make_nanovg_context(NVG_ANTIALIAS | NVG_STENCIL_STROKES);
        if (!nvg) throw std::runtime_error("error initializing nanovg context");
        text_fontface = std::make_shared<NvgFont>(nvg, text_font, read_file_binary("assets/fonts/" + text_font + ".ttf"));
        icon_fontface = std::make_shared<NvgFont>(nvg, icon_font, read_file_binary("assets/fonts/" + icon_font + ".ttf"));
        root = std::make_shared<UIComponent>(stylesheet);
        root->bounds = {0, 0, width, height};
    }
    
    ~UISurface()
    {
        release_nanovg_context(nvg);
    }
    
    UIComponent * get_root() { return root.get(); }
    
    // This should be set before any widgets are added to the root node
    void set_root_stylesheet(UIStyleSheet ss) { stylesheet = ss; }
    
    void handle_input(const InputEvent & event)
    {
        hoverNode = get_hover_component(root, event.cursor);
        
        if (clickedNode)
        {
            if (event.cursor != lastCursor)
                clickedNode->on_mouse_drag(event.cursor, event.cursor - lastCursor);
            
            if (event.type == InputEvent::MOUSE)
            {
                if (event.is_mouse_up())
                {
                    clickedNode->on_mouse_up(event.cursor);
                    clickedNode.reset();
                }
            }

        }
        else
        {
            if (event.type == InputEvent::MOUSE)
            {
                if (event.is_mouse_down())
                {
                    clickedNode = hoverNode;
                    if(clickedNode) clickedNode->on_mouse_down(event.cursor);
                }
            }
        }
        
        lastCursor = event.cursor;
    }
    
    void draw(GLFWwindow * window)
    {
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        nvgBeginFrame(nvg, width, height, 1.0);
        UIRenderEvent e = {nvg, root.get(), text_fontface.get(), icon_fontface.get()};
        render(e, root);
        nvgEndFrame(nvg);
    }
    
    std::shared_ptr<PanelControl> make_panel() const
    {
        auto control = std::make_shared<PanelControl>(stylesheet);
        return control;
    }
    
    std::shared_ptr<LabelControl> make_label(const std::string & text) const
    {
        auto control = std::make_shared<LabelControl>(stylesheet);
        control->set_text(text);
        return control;
    }
    
    std::shared_ptr<ButtonControl> make_button(const std::string & text, bool & variable) const
    {
        auto control = std::make_shared<ButtonControl>(stylesheet);
        control->set_text(text);
        control->set_variable(variable);
        return control;
    }
    
    std::shared_ptr<SliderControl> make_slider(const std::string & text, const float min, const float max, float & variable) const
    {
        auto control = std::make_shared<SliderControl>(stylesheet);
        control->set_text(text);
        control->set_range(min, max);
        control->set_variable(variable);
        return control;
    }

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
    
    std::unique_ptr<UISurface> userInterface;
    std::shared_ptr<PanelControl> leftPanel;
    std::shared_ptr<LabelControl> label;
    std::shared_ptr<ButtonControl> button;
    
    std::shared_ptr<SliderControl> stepSlider;
    std::shared_ptr<SliderControl> fillSlider;
    
    bool btnState = false;
    float sliderVar = 0.0f;
    
    ExperimentalApp() : GLFWApp(940, 720, "Euclidean App")
    {
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
        
        {
            userInterface.reset(new UISurface(width, height, "source_code_pro_regular", "source_code_pro_regular"));
            
            UIStyleSheet stylesheet;
            stylesheet.textColor = nvgRGBA(255, 255, 255, 255);
            stylesheet.backgroundColor = nvgRGBA(30, 30, 30, 255);
            stylesheet.borderColor = nvgRGBA(255, 255, 255, 255);
            
            userInterface->set_root_stylesheet(stylesheet);
            
            leftPanel = userInterface->make_panel();
            label = userInterface->make_label("Debug Panel");
            button = userInterface->make_button("Randomize", btnState);
            stepSlider = userInterface->make_slider("Steps", 0.0f, 1.0f, sliderVar); // fix sliderVar
            fillSlider = userInterface->make_slider("Fills", 0.0f, 1.0f, sliderVar);
            
            leftPanel->add_child({ {0.f,+10}, {0.00f,+10}, {1.f,-10}, {0.25f,-10} }, label);
            leftPanel->add_child({ {0.f,+10}, {0.25f,+10}, {1.f,-10}, {0.50f,-10} }, button);
            leftPanel->add_child({ {0.f,+10}, {0.50f,+10}, {1.f,-10}, {0.75f,-10} }, stepSlider);
            leftPanel->add_child({ {0.f,+10}, {0.75f,+10}, {1.f,-10}, {1.00f,-10} }, fillSlider);
            
            userInterface->get_root()->add_child({ {0.0f,+10}, {0.f,+10}, {0.33f, 0}, {0.50f,0} }, leftPanel);
            
            userInterface->get_root()->layout();
        }
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
        
        gfx::gl_check_error(__FILE__, __LINE__);
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
        userInterface->handle_input(event);
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
        
        userInterface->draw(window);
        
        gfx::gl_check_error(__FILE__, __LINE__);
        
        glfwSwapBuffers(window);
        
        frameCount++;
    }
    
};
