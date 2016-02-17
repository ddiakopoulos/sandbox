#include <functional>
#include <memory>

#include "avl_imgui.hpp"
#include "imgui/imgui_internal.h"

namespace ImGui 
{
    ImGui::Options::Options(const std::shared_ptr<GLFWwindow> & window) : glfwWindowHandle(window)
    {
        build_dark_theme();

        auto renderer                       = getRenderer();
        
        const ImGuiStyle & style            = options.getStyle();
        ImGuiStyle & imGuiStyle             = ImGui::GetStyle();

        imGuiStyle.Alpha                    = style.Alpha;
        imGuiStyle.WindowPadding            = style.WindowPadding;
        imGuiStyle.WindowMinSize            = style.WindowMinSize;
        imGuiStyle.WindowRounding           = style.WindowRounding;
        imGuiStyle.WindowTitleAlign         = style.WindowTitleAlign;
        imGuiStyle.ChildWindowRounding      = style.ChildWindowRounding;
        imGuiStyle.FramePadding             = style.FramePadding;
        imGuiStyle.FrameRounding            = style.FrameRounding;
        imGuiStyle.ItemSpacing              = style.ItemSpacing;
        imGuiStyle.ItemInnerSpacing         = style.ItemInnerSpacing;
        imGuiStyle.TouchExtraPadding        = style.TouchExtraPadding;
        imGuiStyle.WindowFillAlphaDefault   = style.WindowFillAlphaDefault;
        imGuiStyle.IndentSpacing            = style.IndentSpacing;
        imGuiStyle.ColumnsMinSpacing        = style.ColumnsMinSpacing;
        imGuiStyle.ScrollbarSize            = style.ScrollbarSize;
        imGuiStyle.ScrollbarRounding        = style.ScrollbarRounding;
        imGuiStyle.GrabMinSize              = style.GrabMinSize;
        imGuiStyle.GrabRounding             = style.GrabRounding;
        imGuiStyle.DisplayWindowPadding     = style.DisplayWindowPadding;
        imGuiStyle.DisplaySafeAreaPadding   = style.DisplaySafeAreaPadding;
        imGuiStyle.AntiAliasedLines         = style.AntiAliasedLines;
        imGuiStyle.AntiAliasedShapes        = style.AntiAliasedShapes;
        
        for (int i = 0; i < ImGuiCol_COUNT; i++)
        {
            imGuiStyle.Colors[i] = style.Colors[i];
        }

        ImGuiIO & io                        = ImGui::GetIO();

        io.DisplaySize                      = ImVec2(window->getSize().x, window->getSize().y);
        io.DeltaTime                        = 1.0f / 60.0f;

        io.KeyMap[ImGuiKey_Tab]             = GLFW_KEY_TAB;
        io.KeyMap[ImGuiKey_LeftArrow]       = GLFW_KEY_LEFT;
        io.KeyMap[ImGuiKey_RightArrow]      = GLFW_KEY_RIGHT;
        io.KeyMap[ImGuiKey_UpArrow]         = GLFW_KEY_UP;
        io.KeyMap[ImGuiKey_DownArrow]       = GLFW_KEY_DOWN;
        io.KeyMap[ImGuiKey_Home]            = GLFW_KEY_HOME;
        io.KeyMap[ImGuiKey_End]             = GLFW_KEY_END;
        io.KeyMap[ImGuiKey_Delete]          = GLFW_KEY_DELETE;
        io.KeyMap[ImGuiKey_Backspace]       = GLFW_KEY_BACKSPACE;
        io.KeyMap[ImGuiKey_Enter]           = GLFW_KEY_RETURN;
        io.KeyMap[ImGuiKey_Escape]          = GLFW_KEY_ESCAPE;
        io.KeyMap[ImGuiKey_A]               = GLFW_KEY_A;
        io.KeyMap[ImGuiKey_C]               = GLFW_KEY_C;
        io.KeyMap[ImGuiKey_V]               = GLFW_KEY_V;
        io.KeyMap[ImGuiKey_X]               = GLFW_KEY_C;
        io.KeyMap[ImGuiKey_Y]               = GLFW_KEY_Y;
        io.KeyMap[ImGuiKey_Z]               = GLFW_KEY_Z;
        
        // Fonts
        ImFontAtlas* fontAtlas  = ImGui::GetIO().Fonts;
        fontAtlas->Clear();
        for (auto font : options.getFonts())
        {
            string name = font.first.stem().string();
            renderer->add_font(loadFile(font.first), font.second, options.getFontGlyphRanges(name) );
        }
        renderer->initialize_font_texture();
        
        /*
        // Clipboard
        io.SetClipboardTextFn = [](const char* text)
        {
            const char* text_end = text + strlen(text);
            char* buf = (char*)malloc(text_end - text + 1);
            memcpy(buf, text, text_end-text);
            buf[text_end-text] = '\0';
            Clipboard::setString(buf);
            free(buf);
        };

        io.GetClipboardTextFn = []()
        {
            string str = Clipboard::getString();
            static vector<char> strCopy;
            strCopy = vector<char>(str.begin(), str.end());
            strCopy.push_back('\0');
            return (const char *) &strCopy[0];
        };
        */
        
        // renderer callback
        io.RenderDrawListsFn = [](ImDrawData* data) 
        {
            auto renderer = getRenderer();
            renderer->render(data);
        };
        
        // connect window's signals
        connectWindow(window);
        
        if (options.isAutoRenderEnabled() && window) 
        {
            ImGui::NewFrame();
            sWindowConnections.push_back(window->getSignalPostDraw().connect(render));
        }
        
        // connect app's signals
        app::App::get()->getSignalDidBecomeActive().connect(resetKeys);
        app::App::get()->getSignalWillResignActive().connect(resetKeys);
    }

    ImGui::Options & ImGui::Options::autoRender(bool autoRender)
    {
        mAutoRender = autoRender; return *this;
    }

    ImGui::Options & ImGui::Options::font(const std::string &fontPath, float size)
    {
        mFonts = { { fontPath, size } }; return *this;
    }

    ImGui::Options & ImGui::Options::fonts(const std::vector<std::pair<std::string,float>> &fonts)
    {
        mFonts = fonts; return *this;
    }

    ImGui::Options & ImGui::Options::fontGlyphRanges(const std::string &name, const vector<ImWchar> &glyphRanges)
    {
        mFontsGlyphRanges[name] = glyphRanges; return *this;
    }

    ImGui::Options & ImGui::Options::alpha(float a)
    {
        mStyle.Alpha = a; return *this;
    }

    ImGui::Options & ImGui::Options::windowPadding(const float2 & padding)
    {
        mStyle.WindowPadding = padding; return *this;
    }

    ImGui::Options & ImGui::Options::windowMinSize(const float2 & minSize)
    {
        mStyle.WindowMinSize = minSize; return *this;
    }

    ImGui::Options & ImGui::Options::windowRounding(float rounding)
    {
        mStyle.WindowRounding = rounding; return *this;
    }

    ImGui::Options & ImGui::Options::windowTitleAlign(ImGuiAlign align)
    {
        mStyle.WindowTitleAlign = align; return *this;
    }

    ImGui::Options & ImGui::Options::childWindowRounding(float rounding)
    {
        mStyle.ChildWindowRounding = rounding; return *this;
    }

    ImGui::Options & ImGui::Options::framePadding(const float2 & padding)
    {
        mStyle.FramePadding = padding; return *this;
    }

    ImGui::Options & ImGui::Options::frameRounding(float rounding)
    {
        mStyle.FrameRounding = rounding; return *this;
    }

    ImGui::Options & ImGui::Options::itemSpacing(const float2 & spacing)
    {
        mStyle.ItemSpacing = spacing; return *this;
    }

    ImGui::Options & ImGui::Options::itemInnerSpacing(const float2 & spacing)
    {
        mStyle.ItemInnerSpacing = spacing; return *this;
    }

    ImGui::Options & ImGui::Options::touchExtraPadding(const float2 & padding)
    {
        mStyle.TouchExtraPadding = padding; return *this;
    }

    ImGui::Options & ImGui::Options::windowFillAlphaDefault(float defaultAlpha)
    {
        mStyle.WindowFillAlphaDefault = defaultAlpha; return *this;
    }

    ImGui::Options & ImGui::Options::indentSpacing(float spacing)
    {
        mStyle.IndentSpacing = spacing; return *this;
    }

    ImGui::Options & ImGui::Options::columnsMinSpacing(float minSpacing)
    {
        mStyle.ColumnsMinSpacing = minSpacing; return *this;
    }

    ImGui::Options & ImGui::Options::scrollBarSize(float size)
    {
        mStyle.ScrollbarSize = size; return *this;
    }

    ImGui::Options & ImGui::Options::scrollbarRounding(float rounding)
    {
        mStyle.ScrollbarRounding = rounding; return *this;
    }

    ImGui::Options & ImGui::Options::grabMinSize(float minSize)
    {
        mStyle.GrabMinSize = minSize; return *this;
    }

    ImGui::Options & ImGui::Options::grabRounding(float rounding)
    {
        mStyle.GrabRounding = rounding; return *this;
    }

    ImGui::Options & ImGui::Options::displayWindowPadding(const float2 & padding)
    {
        mStyle.DisplayWindowPadding = padding; return *this;
    }

    ImGui::Options & ImGui::Options::displaySafeAreaPadding(const float2 & padding)
    {
        mStyle.DisplaySafeAreaPadding = padding; return *this;
    }

    ImGui::Options & ImGui::Options::antiAliasedLines(bool antiAliasing)
    {
        mStyle.AntiAliasedLines = antiAliasing; return *this;
    }

    ImGui::Options & ImGui::Options::antiAliasedShapes(bool antiAliasing)
    {
        mStyle.AntiAliasedShapes = antiAliasing; return *this;
    }

    ImGui::Options & ImGui::Options::curveTessellationTol(float tessTolerance)
    {
        mStyle.CurveTessellationTol = tessTolerance; return *this;
    }

    const ImWchar* ImGui::Options::getFontGlyphRanges(const std::string &name) const
    {
        if (mFontsGlyphRanges.count(name)) return &mFontsGlyphRanges.find(name)->second[0];
        else return NULL;
    }

    ImGui::Options & ImGui::Options::defaultTheme()
    {
        mStyle = ImGuiStyle(); return *this;
    }

    ImGui::Options & ImGui::Options::build_dark_theme()
    {
        mStyle.WindowMinSize                        = ImVec2(160, 20);
        mStyle.FramePadding                         = ImVec2(4, 2);
        mStyle.ItemSpacing                          = ImVec2(6, 2);
        mStyle.ItemInnerSpacing                     = ImVec2(6, 4);
        mStyle.Alpha                                = 0.95f;
        mStyle.WindowFillAlphaDefault               = 1.0f;
        mStyle.WindowRounding                       = 4.0f;
        mStyle.FrameRounding                        = 2.0f;
        mStyle.IndentSpacing                        = 6.0f;
        mStyle.ItemInnerSpacing                     = ImVec2(2, 4);
        mStyle.ColumnsMinSpacing                    = 50.0f;
        mStyle.GrabMinSize                          = 14.0f;
        mStyle.GrabRounding                         = 16.0f;
        mStyle.ScrollbarSize                        = 12.0f;
        mStyle.ScrollbarRounding                    = 16.0f;
        
        ImGuiStyle & style = mStyle;
        style.Colors[ImGuiCol_Text]                  = ImVec4(0.86f, 0.93f, 0.89f, 0.61f);
        style.Colors[ImGuiCol_TextDisabled]          = ImVec4(0.86f, 0.93f, 0.89f, 0.28f);
        style.Colors[ImGuiCol_WindowBg]              = ImVec4(0.13f, 0.14f, 0.17f, 1.00f);
        style.Colors[ImGuiCol_ChildWindowBg]         = ImVec4(0.20f, 0.22f, 0.27f, 0.58f);
        style.Colors[ImGuiCol_Border]                = ImVec4(0.31f, 0.31f, 1.00f, 0.00f);
        style.Colors[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        style.Colors[ImGuiCol_FrameBg]               = ImVec4(0.20f, 0.22f, 0.27f, 1.00f);
        style.Colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.92f, 0.18f, 0.29f, 0.78f);
        style.Colors[ImGuiCol_FrameBgActive]         = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
        style.Colors[ImGuiCol_TitleBg]               = ImVec4(0.20f, 0.22f, 0.27f, 1.00f);
        style.Colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.20f, 0.22f, 0.27f, 0.75f);
        style.Colors[ImGuiCol_TitleBgActive]         = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
        style.Colors[ImGuiCol_MenuBarBg]             = ImVec4(0.20f, 0.22f, 0.27f, 0.47f);
        style.Colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.20f, 0.22f, 0.27f, 1.00f);
        style.Colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.47f, 0.77f, 0.83f, 0.21f);
        style.Colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.92f, 0.18f, 0.29f, 0.78f);
        style.Colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
        style.Colors[ImGuiCol_ComboBg]               = ImVec4(0.20f, 0.22f, 0.27f, 1.00f);
        style.Colors[ImGuiCol_CheckMark]             = ImVec4(0.71f, 0.22f, 0.27f, 1.00f);
        style.Colors[ImGuiCol_SliderGrab]            = ImVec4(0.47f, 0.77f, 0.83f, 0.14f);
        style.Colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
        style.Colors[ImGuiCol_Button]                = ImVec4(0.47f, 0.77f, 0.83f, 0.14f);
        style.Colors[ImGuiCol_ButtonHovered]         = ImVec4(0.92f, 0.18f, 0.29f, 0.86f);
        style.Colors[ImGuiCol_ButtonActive]          = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
        style.Colors[ImGuiCol_Header]                = ImVec4(0.92f, 0.18f, 0.29f, 0.76f);
        style.Colors[ImGuiCol_HeaderHovered]         = ImVec4(0.92f, 0.18f, 0.29f, 0.86f);
        style.Colors[ImGuiCol_HeaderActive]          = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
        style.Colors[ImGuiCol_Column]                = ImVec4(0.47f, 0.77f, 0.83f, 0.32f);
        style.Colors[ImGuiCol_ColumnHovered]         = ImVec4(0.92f, 0.18f, 0.29f, 0.78f);
        style.Colors[ImGuiCol_ColumnActive]          = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
        style.Colors[ImGuiCol_ResizeGrip]            = ImVec4(0.47f, 0.77f, 0.83f, 0.04f);
        style.Colors[ImGuiCol_ResizeGripHovered]     = ImVec4(0.92f, 0.18f, 0.29f, 0.78f);
        style.Colors[ImGuiCol_ResizeGripActive]      = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
        style.Colors[ImGuiCol_CloseButton]           = ImVec4(0.86f, 0.93f, 0.89f, 0.16f);
        style.Colors[ImGuiCol_CloseButtonHovered]    = ImVec4(0.86f, 0.93f, 0.89f, 0.39f);
        style.Colors[ImGuiCol_CloseButtonActive]     = ImVec4(0.86f, 0.93f, 0.89f, 1.00f);
        style.Colors[ImGuiCol_PlotLines]             = ImVec4(0.86f, 0.93f, 0.89f, 0.63f);
        style.Colors[ImGuiCol_PlotLinesHovered]      = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
        style.Colors[ImGuiCol_PlotHistogram]         = ImVec4(0.86f, 0.93f, 0.89f, 0.63f);
        style.Colors[ImGuiCol_PlotHistogramHovered]  = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
        style.Colors[ImGuiCol_TextSelectedBg]        = ImVec4(0.92f, 0.18f, 0.29f, 0.43f);
        style.Colors[ImGuiCol_TooltipBg]             = ImVec4(0.47f, 0.77f, 0.83f, 0.72f);
        style.Colors[ImGuiCol_ModalWindowDarkening]  = ImVec4(0.20f, 0.22f, 0.27f, 0.73f);
        return *this;
    }

    ImGui::Options & ImGui::Options::color(ImGuiCol option, const float4 & color)
    {
        mStyle.Colors[option] = color; return *this;
    }

    class Renderer 
    {
        GlTexture mFontTexture;
        GlShader mShader;

        ci::gl::VaoRef mVao;
        ci::gl::VboRef mVbo;
        ci::gl::VboRef mIbo;

        std::map<string,ImFont*> mFonts;

    public:

        Renderer();
        
        void render(ImDrawData* draw_data);

        ImFont * add_font(std::vector<uint8_t> & font, float size, const ImWchar* glyph_ranges = NULL);

        GlTexture & get_font_texture();

        ci::gl::VaoRef getVao();
        ci::gl::VboRef getVbo();

        GlShader & get_shader();
        
        void initialize_font_texture();
        void initialize_buffers(size_t size = 1000);
        void create_gl_program();
        
        ImFont * getFont(const std::string &name);
    };

    Renderer::Renderer()
    {
        create_gl_program();
        initialize_buffers();
    }

    // renders imgui drawlist
    void Renderer::render(ImDrawData* draw_data)
    {
        const float w = ImGui::GetIO().DisplaySize.x;
        const float h = ImGui::GetIO().DisplaySize.y;

        const auto vbo = getVbo();
        const auto shader = get_shader();
        
        const float4x4 modelViewMat = {
            {  2.0f/w, 0.0f,     0.0f,  0.0f },
            {  0.0f,   2.0f/-h,  0.0f,  0.0f },
            {  0.0f,   0.0f,    -1.0f,  0.0f },
            { -1.0f,   1.0f,     0.0f,  1.0f },
        };

        shader.enable();

        shader->uniform("u_mvp", modelViewMat);
        shader->uniform("u_texture", 0);
        
        for (int n = 0; n < draw_data->CmdListsCount; n++) 
        {
            const ImDrawList* cmd_list = draw_data->CmdLists[n];
            const ImDrawIdx* idx_buffer = &cmd_list->IdxBuffer.front();
            
            // Resize VBO
            int needed_vtx_size = cmd_list->VtxBuffer.size() * sizeof(ImDrawVert);
            if (vbo->getSize() < needed_vtx_size)
            {
                GLsizeiptr size = needed_vtx_size + 2000 * sizeof(ImDrawVert);
                mVbo->bufferData(size, nullptr, GL_STREAM_DRAW);
            }
            
            // Update VBO
            {
                gl::ScopedBuffer scopedVbo(GL_ARRAY_BUFFER, vbo->get_gl_handle());
                ImDrawVert *vtx_data = static_cast<ImDrawVert*>(vbo->mapReplace());
                if (!vtx_data) continue;
                memcpy(vtx_data, &cmd_list->VtxBuffer[0], cmd_list->VtxBuffer.size() * sizeof(ImDrawVert));
                vbo->unmap();
            }
            
            // Draw
            for (const ImDrawCmd* pcmd = cmd_list->CmdBuffer.begin(); pcmd != cmd_list->CmdBuffer.end(); pcmd++) 
            {
                if (pcmd->UserCallback) 
                {
                    pcmd->UserCallback(cmd_list, pcmd);
                }
                else 
                {
                    gl::ScopedVao scopedVao(getVao().get());
                    gl::ScopedBuffer scopedIndexBuffer(mIbo);

                    gl::ScopedTextureBind scopedTexture(GL_TEXTURE_2D, (GLuint)(intptr_t) pcmd->TextureId);
                    gl::ScopedScissor scopedScissors((int)pcmd->ClipRect.x, (int)(height - pcmd->ClipRect.w), (int)(pcmd->ClipRect.z - pcmd->ClipRect.x), (int)(pcmd->ClipRect.w - pcmd->ClipRect.y));
                    gl::ScopedDepth scopedDepth(false);

                    gl::ScopedBlendAlpha scopedBlend;

                    gl::ScopedFaceCulling scopedFaceCulling(false);
                    
                    mIbo->bufferData(pcmd->ElemCount * sizeof(ImDrawIdx), idx_buffer, GL_STREAM_DRAW);
                    gl::drawElements(GL_TRIANGLES, (GLsizei) pcmd->ElemCount, GL_UNSIGNED_SHORT, nullptr);
                }
                idx_buffer += pcmd->ElemCount;
            }
        }

        shader.disable();
    }

    GlTexture & Renderer::get_font_texture()
    {
        if (!mFontTexture) initialize_font_texture();
        return mFontTexture;
    }

    gl::VaoRef Renderer::getVao()
    {
        if (!mVao) initialize_buffers();
        return mVao;
    }

    gl::VboRef Renderer::getVbo()
    {
        if (!mVbo) initialize_buffers();
        return mVbo;
    }

    void Renderer::initialize_buffers(size_t size)
    {
        mVbo = gl::Vbo::create(GL_ARRAY_BUFFER, size, nullptr, GL_STREAM_DRAW);
        mIbo = gl::Vbo::create(GL_ELEMENT_ARRAY_BUFFER, 10, nullptr, GL_STREAM_DRAW);
        mVao = gl::Vao::create();
        
        gl::ScopedVao mVaoScope(mVao);
        gl::ScopedBuffer mVboScope(mVbo);
        
        gl::enableVertexAttribArray(0);
        gl::enableVertexAttribArray(1);
        gl::enableVertexAttribArray(2);
        
        gl::vertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), (const GLvoid *)offsetof(ImDrawVert, pos));
        gl::vertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), (const GLvoid *)offsetof(ImDrawVert, uv));
        gl::vertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(ImDrawVert), (const GLvoid *)offsetof(ImDrawVert, col));
    }

    void Renderer::initialize_font_texture()
    {
        unsigned char * pixels;
        int width, height;

        ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

        mFontTexture = gl::Texture::create(pixels, GL_RGBA, width, height, gl::Texture::Format().magFilter(GL_LINEAR).minFilter(GL_LINEAR));

        ImGui::GetIO().Fonts->ClearTexData();
        ImGui::GetIO().Fonts->TexID = (void *)(intptr_t) mFontTexture->get_gl_handle();
    }

    ImFont* Renderer::add_font(std::vector<uint8_t> & font, float size, const ImWchar * glyph_ranges)
    {
        ImFontAtlas* fontAtlas = ImGui::GetIO().Fonts;
        
        ImFontConfig config;
        ImFont * newFont = fontAtlas->Add_fontFromMemoryTTF(font.data(), font.size(), size, &config, glyph_ranges);
        
        // Fixme
        mFonts.insert(make_pair(font->getFilePath().stem().string(), newFont));
        return newFont;
    }

    GlShader & Renderer::get_shader()
    {
        if (!mShader) create_gl_program();
        return mShader;
    }

    void Renderer::create_gl_program()
    {
        constexpr const char colorVertexShader[] = R"(#version 330
            uniform mat4 u_mvp;
            in vec2      inPosition;
            in vec2      inTexcoord;
            in vec4      inColor;
            out vec2     vUv;
            out vec4     vColor;
            void main() 
            {
                vColor       = inColor;
                vUv          = inTexcoord;
                gl_Position  = u_mvp * vec4(inPosition, 0.0, 1.0);
            }
        )";

        constexpr const char colorFragmentShader[] = R"(#version 330
            in vec2             vUv;
            in vec4             vColor;
            uniform sampler2D   u_texture;
            
            out vec4 f_color;

            void main() 
            {
                vec4 color = texture(u_texture, vUv) * vColor;
                f_color = color;
            }
        )";

        try 
        {
            mShader = GlShader(colorVertexShader, colorFragmentShader);
            glBindAttribLocation(mShader.get_handle(), 0, "inPosition");
            glBindAttribLocation(mShader.get_handle(), 1, "inTexcoord");
            glBindAttribLocation(mShader.get_handle(), 2, "inColor");
        }
        catch (const std::exception & e)
        {
            throw std::runtime_error("Problem Compiling ImGui::Renderer shader " + e.what());
        }
    }

    ImFont* Renderer::getFont(const std::string &name)
    {
        if (!mFonts.count(name)) return nullptr;
        else return mFonts[name];
    }

    std::shared_ptr<Renderer> getRenderer()
    {  
        // Fuck! Get rid of static
        static std::shared_ptr<Renderer> renderer = std::make_shared<Renderer>();
        return renderer;
    }

    //////////////////////////////
    //   Helper Functionality   //
    //////////////////////////////

    void Image(const GlTexture &texture, const ImVec2 & size, const ImVec2 & uv0, const ImVec2 & uv1, const ImVec4 & tint_col, const ImVec4 & border_col)
    {
        Image((void *)(intptr_t) texture->get_gl_handle(), size, uv0, uv1, tint_col, border_col);
    }

    bool ImageButton(const GlTexture &texture, const ImVec2 & size, const ImVec2 & uv0, const ImVec2 & uv1, int frame_padding, const ImVec4 & bg_col, const ImVec4 & tint_col)
    {
        return ImageButton((void *)(intptr_t) texture->get_gl_handle(), size, uv0, uv1, frame_padding, bg_col, tint_col);
    }

    void PushFont(const std::string& name)
    {
        auto renderer = getRenderer();
        ImFont * font = renderer->getFont(name);
        assert(font != nullptr);
        PushFont(font);
    }

    bool ListBox(const char* label, int * current_item, const std::vector<std::string> & items, int height_in_items)
    {
        std::vector<const char*> names;
        for (auto item : items) 
        {
            char *cname = new char[item.size() + 1];
            std::strcpy(cname, item.c_str());
            names.push_back(cname);
        }
        bool result = ListBox(label, current_item, names.data(), names.size(), height_in_items);
        for (auto &name : names) delete [] name;
        return result;
    }

    bool InputText(const char* label, std::string* buf, ImGuiInputTextFlags flags, ImGuiTextEditCallback callback, void * user_data)
    {
        char *buffer = new char[buf->size()+128];
        std::strcpy(buffer, buf->c_str());
        bool result = InputText(label, buffer, buf->size()+128, flags, callback, user_data);
        if (result) *buf = string(buffer);
        delete [] buffer;
        return result;
    }

    bool InputTextMultiline(const char* label, std::string* buf, const ImVec2 & size, ImGuiInputTextFlags flags, ImGuiTextEditCallback callback, void * user_data)
    {
        char *buffer = new char[buf->size()+128];
        std::strcpy(buffer, buf->c_str());
        bool result = InputTextMultiline(label, buffer, buf->size()+128, size, flags, callback, user_data);
        if (result) *buf = string(buffer);
        delete [] buffer;
        return result;
    }

    bool Combo(const char* label, int * current_item, const std::vector<std::string> & items, int height_in_items)
    {
        string itemsNames;
        for (auto item : items) itemsNames += item + '\0';
        itemsNames += '\0';
        std::vector<char> charArray(itemsNames.begin(), itemsNames.end());
        bool result = Combo(label, current_item, (const char*) &charArray[0], height_in_items);
        return result;
    }

                case InputEvent::MOUSE:
                    switch (e.value[0])
                {
                    case GLFW_MOUSE_BUTTON_LEFT: ml = e.is_mouse_down(); break;
                    case GLFW_MOUSE_BUTTON_RIGHT: mr = e.is_mouse_down(); break;
                }


    namespace 
    {
        
        void mouseDown(avl::InputEvent event)
        {
            ImGuiIO & io = ImGui::GetIO();
            io.MousePos = event.cursor;

            if (event.type == InputEvent::MOUSE)
            {
                if (event.value[0] == GLFW_MOUSE_BUTTON_LEFT && event.is_mouse_down())
                {
                    io.MouseDown[0] = true;
                    io.MouseDown[1] = false;
                }
                if (event.value[0] == GLFW_MOUSE_BUTTON_RIGHT && event.is_mouse_down())
                {
                    io.MouseDown[0] = false;
                    io.MouseDown[1] = true;
                }
            }
        }

        void mouseMove(avl::InputEvent event)
        {
            ImGuiIO & io = ImGui::GetIO();
            io.MousePos = event.cursor;
        }

        void mouseDrag(avl::InputEvent event)
        {
            ImGuiIO & io = ImGui::GetIO();
            io.MousePos = event.cursor;
        }

        void mouseUp(avl::InputEvent event)
        {
            ImGuiIO & io = ImGui::GetIO();
            io.MouseDown[0] = false;
            io.MouseDown[1] = false;
        }

        void mouseWheel(avl::InputEvent event)
        {
            ImGuiIO & io = ImGui::GetIO();
            // Fixme - scroll delta? 
            io.MouseWheel += event.value[0];
        }
        
        vector<int> sAccelKeys;
        
        // sets the right keyDown IO values in imgui
        void keyDown(avl::InputEvent event)
        {
            ImGuiIO & io = ImGui::GetIO();
            
            uint32_t character = event.getCharUtf32();
            
            io.KeysDown[event.value[0]] = true;
            
            if (!event.isAccelDown() && character > 0 && character <= 255) 
            {
                io.AddInputCharacter((char) character);
            }
            else if (event.value[0] != GLFW_KEY_LMETA
                && event.value[0] != GLFW_KEY_RMETA
                && event.isAccelDown()
                && find(sAccelKeys.begin(), sAccelKeys.end(), event.value[0]) == sAccelKeys.end())
            {
                sAccelKeys.push_back(event.value[0]);
            }
            
            io.KeyCtrl = io.KeysDown[GLFW_KEY_LCTRL] || io.KeysDown[GLFW_KEY_RCTRL] || io.KeysDown[GLFW_KEY_LMETA] || io.KeysDown[GLFW_KEY_RMETA];
            io.KeyShift = io.KeysDown[GLFW_KEY_LSHIFT] || io.KeysDown[GLFW_KEY_RSHIFT];
            io.KeyAlt = io.KeysDown[GLFW_KEY_LALT] || io.KeysDown[GLFW_KEY_RALT];
        }

        // sets the right keyUp IO values in imgui
        void keyUp(avl::InputEvent event)
        {
            ImGuiIO & io = ImGui::GetIO();
            
            io.KeysDown[event.value[0]] = false;
            
            for (auto key : sAccelKeys)
                io.KeysDown[key] = false;

            sAccelKeys.clear();
            
            io.KeyCtrl = io.KeysDown[GLFW_KEY_LCTRL] || io.KeysDown[GLFW_KEY_RCTRL] || io.KeysDown[GLFW_KEY_LMETA] || io.KeysDown[GLFW_KEY_RMETA];
            io.KeyShift = io.KeysDown[GLFW_KEY_LSHIFT] || io.KeysDown[GLFW_KEY_RSHIFT];
            io.KeyAlt = io.KeysDown[GLFW_KEY_LALT] || io.KeysDown[GLFW_KEY_RALT];
        }

        void resize()
        {
            ImGuiIO & io = ImGui::GetIO();
            io.DisplaySize = toPixels(getWindowSize());
        }
        
        static bool sNewFrame = false;

        void render()
        {
            ImGuiIO & io = ImGui::GetIO();
            io.DeltaTime = FIXME_SECONDS;
            ImGui::Render();
            ImGui::NewFrame();
        }
        
        void resetKeys()
        {
            for (int i = 0; i < IM_ARRAYSIZE(ImGui::GetIO().KeysDown); i++) 
                ImGui::GetIO().KeysDown[i] = false;

            for (int i = 0; i < IM_ARRAYSIZE(ImGui::GetIO().KeysDownDuration); i++) 
                ImGui::GetIO().KeysDownDuration[i] = 0.0f;

            for (int i = 0; i < IM_ARRAYSIZE(ImGui::GetIO().KeysDownDurationPrev); i++) 
                ImGui::GetIO().KeysDownDurationPrev[i] = 0.0f;

            ImGui::GetIO().KeyCtrl = false;
            ImGui::GetIO().KeyShift = false;
            ImGui::GetIO().KeyAlt = false;
        }
        
    } 

    ////////////////////////////////
    //   Scoped ImGui Utilities   //
    ////////////////////////////////

    ScopedWindow::ScopedWindow(const std::string &name, ImGuiWindowFlags flags) { ImGui::Begin(name.c_str(), nullptr, flags); }

    ScopedWindow::ScopedWindow(const std::string &name, float2 size, float fillAlpha, ImGuiWindowFlags flags) { ImGui::Begin(name.c_str(), nullptr, size, fillAlpha, flags); }

    ScopedWindow::~ScopedWindow() { ImGui::End(); }

    ScopedChild::ScopedChild(const std::string &name, float2 size, bool border, ImGuiWindowFlags extraFlags) { ImGui::BeginChild(name.c_str(), size, border, extraFlags); }

    ScopedChild::~ScopedChild() { ImGui::EndChild(); }

    ScopedGroup::ScopedGroup() { ImGui::BeginGroup(); }

    ScopedGroup::~ScopedGroup() { ImGui::EndGroup(); }

    ScopedFont::ScopedFont(ImFont* font) { ImGui::PushFont(font); }

    ScopedFont::ScopedFont(const std::string &name) { ImGui::PushFont(name); }

    ScopedFont::~ScopedFont() { ImGui::PopFont(); }

    ScopedStyleColor::ScopedStyleColor(ImGuiCol idx, const ImVec4 & col) { ImGui::PushStyleColor(idx, col); }

    ScopedStyleColor::~ScopedStyleColor() { ImGui::PopStyleColor(); }

    ScopedStyleVar::ScopedStyleVar(ImGuiStyleVar idx, float val) { ImGui::PushStyleVar(idx, val); }

    ScopedStyleVar::ScopedStyleVar(ImGuiStyleVar idx, const ImVec2 &val) { ImGui::PushStyleVar(idx, val); }

    ScopedStyleVar::~ScopedStyleVar() { ImGui::PopStyleVar(); }

    ScopedItemWidth::ScopedItemWidth(float itemWidth) { ImGui::PushItemWidth(itemWidth); }

    ScopedItemWidth::~ScopedItemWidth() { ImGui::PopItemWidth(); }

    ScopedTextWrapPos::ScopedTextWrapPos(float wrapPosX) { ImGui::PushTextWrapPos(wrapPosX); }

    ScopedTextWrapPos::~ScopedTextWrapPos() { ImGui::PopTextWrapPos(); }

    ScopedId::ScopedId(const std::string &name) { ImGui::PushID(name.c_str()); }

    ScopedId::ScopedId(const void *ptrId) { ImGui::PushID(ptrId); }

    ScopedId::ScopedId(const int intId) { ImGui::PushID(intId); }

    ScopedId::~ScopedId() { ImGui::PopID(); }

    ScopedMainMenuBar::ScopedMainMenuBar() { mOpened = ImGui::BeginMainMenuBar(); }

    ScopedMainMenuBar::~ScopedMainMenuBar() { if (mOpened) ImGui::EndMainMenuBar(); }

    ScopedMenuBar::ScopedMenuBar() { mOpened = ImGui::BeginMenuBar(); }

    ScopedMenuBar::~ScopedMenuBar() { if (mOpened) ImGui::EndMenuBar(); }

}