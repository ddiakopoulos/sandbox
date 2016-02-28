#include <functional>
#include <memory>

#include "avl_imgui.hpp"
#include "imgui/imgui_internal.h"

#include "GL_API.hpp"
#include "glfw_app.hpp"

// Implement singleton
template<> gui::ImGuiApp * Singleton<gui::ImGuiApp>::single = nullptr;

using namespace avl;

namespace gui
{
    ///////////////////////////////////////
    //   ImGui Renderer Implementation   //
    ///////////////////////////////////////
    
    // https://github.com/ocornut/imgui/tree/master/examples/opengl3_example
    
    ImGuiManager::ImGuiManager(GLFWwindow * win)
    {
        ImGuiApp & state = ImGuiApp::get_instance();
        
        state.window = win;
        
        ImGuiIO & io = ImGui::GetIO();
        io.KeyMap[ImGuiKey_Tab] = GLFW_KEY_TAB;
        io.KeyMap[ImGuiKey_LeftArrow] = GLFW_KEY_LEFT;
        io.KeyMap[ImGuiKey_RightArrow] = GLFW_KEY_RIGHT;
        io.KeyMap[ImGuiKey_UpArrow] = GLFW_KEY_UP;
        io.KeyMap[ImGuiKey_DownArrow] = GLFW_KEY_DOWN;
        io.KeyMap[ImGuiKey_PageUp] = GLFW_KEY_PAGE_UP;
        io.KeyMap[ImGuiKey_PageDown] = GLFW_KEY_PAGE_DOWN;
        io.KeyMap[ImGuiKey_Home] = GLFW_KEY_HOME;
        io.KeyMap[ImGuiKey_End] = GLFW_KEY_END;
        io.KeyMap[ImGuiKey_Delete] = GLFW_KEY_DELETE;
        io.KeyMap[ImGuiKey_Backspace] = GLFW_KEY_BACKSPACE;
        io.KeyMap[ImGuiKey_Enter] = GLFW_KEY_ENTER;
        io.KeyMap[ImGuiKey_Escape] = GLFW_KEY_ESCAPE;
        io.KeyMap[ImGuiKey_A] = GLFW_KEY_A;
        io.KeyMap[ImGuiKey_C] = GLFW_KEY_C;
        io.KeyMap[ImGuiKey_V] = GLFW_KEY_V;
        io.KeyMap[ImGuiKey_X] = GLFW_KEY_X;
        io.KeyMap[ImGuiKey_Y] = GLFW_KEY_Y;
        io.KeyMap[ImGuiKey_Z] = GLFW_KEY_Z;
        
        io.Fonts->AddFontDefault();
        
        io.RenderDrawListsFn = [](ImDrawData * draw_data)
        {
            ImGuiApp & s = ImGuiApp::get_instance();
            
            // Backup GL state
            GLint last_program; glGetIntegerv(GL_CURRENT_PROGRAM, &last_program);
            GLint last_texture; glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
            GLint last_array_buffer; glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &last_array_buffer);
            GLint last_element_array_buffer; glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &last_element_array_buffer);
            GLint last_vertex_array; glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &last_vertex_array);
            GLint last_blend_src; glGetIntegerv(GL_BLEND_SRC, &last_blend_src);
            GLint last_blend_dst; glGetIntegerv(GL_BLEND_DST, &last_blend_dst);
            GLint last_blend_equation_rgb; glGetIntegerv(GL_BLEND_EQUATION_RGB, &last_blend_equation_rgb);
            GLint last_blend_equation_alpha; glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &last_blend_equation_alpha);
            GLint last_viewport[4]; glGetIntegerv(GL_VIEWPORT, last_viewport);
            GLboolean last_enable_blend = glIsEnabled(GL_BLEND);
            GLboolean last_enable_cull_face = glIsEnabled(GL_CULL_FACE);
            GLboolean last_enable_depth_test = glIsEnabled(GL_DEPTH_TEST);
            GLboolean last_enable_scissor_test = glIsEnabled(GL_SCISSOR_TEST);
            
            // Setup render state: alpha-blending enabled, no face culling, no depth testing, scissor enabled
            glEnable(GL_BLEND);
            glBlendEquation(GL_FUNC_ADD);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDisable(GL_CULL_FACE);
            glDisable(GL_DEPTH_TEST);
            glEnable(GL_SCISSOR_TEST);
            glActiveTexture(GL_TEXTURE0);
            
            // Handle cases of screen coordinates != from framebuffer coordinates (e.g. retina displays)
            ImGuiIO& io = ImGui::GetIO();
            int fb_width = (int)(io.DisplaySize.x * io.DisplayFramebufferScale.x);
            int fb_height = (int)(io.DisplaySize.y * io.DisplayFramebufferScale.y);
            if (fb_width == 0 || fb_height == 0) return;
            draw_data->ScaleClipRects(io.DisplayFramebufferScale);
            
            // Setup viewport, orthographic projection matrix
            glViewport(0, 0, (GLsizei)fb_width, (GLsizei)fb_height);
            const float ortho_projection[4][4] = {
                { 2.0f/io.DisplaySize.x, 0.0f,                   0.0f, 0.0f },
                { 0.0f,                  2.0f/-io.DisplaySize.y, 0.0f, 0.0f },
                { 0.0f,                  0.0f,                  -1.0f, 0.0f },
                {-1.0f,                  1.0f,                   0.0f, 1.0f },
            };
            
            glUseProgram(s.ShaderHandle);
            glUniform1i(s.AttribLocationTex, 0);
            glUniformMatrix4fv(s.AttribLocationProjMtx, 1, GL_FALSE, &ortho_projection[0][0]);
            glBindVertexArray(s.VaoHandle);
            
            for (int n = 0; n < draw_data->CmdListsCount; n++)
            {
                const ImDrawList* cmd_list = draw_data->CmdLists[n];
                const ImDrawIdx* idx_buffer_offset = 0;
                
                glBindBuffer(GL_ARRAY_BUFFER, s.VboHandle);
                glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)cmd_list->VtxBuffer.size() * sizeof(ImDrawVert), (GLvoid*)&cmd_list->VtxBuffer.front(), GL_STREAM_DRAW);
                
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s.ElementsHandle);
                glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)cmd_list->IdxBuffer.size() * sizeof(ImDrawIdx), (GLvoid*)&cmd_list->IdxBuffer.front(), GL_STREAM_DRAW);
                
                for (const ImDrawCmd* pcmd = cmd_list->CmdBuffer.begin(); pcmd != cmd_list->CmdBuffer.end(); pcmd++)
                {
                    if (pcmd->UserCallback)
                    {
                        pcmd->UserCallback(cmd_list, pcmd);
                    }
                    else
                    {
                        glBindTexture(GL_TEXTURE_2D, (GLuint)(intptr_t)pcmd->TextureId);
                        glScissor((int)pcmd->ClipRect.x, (int)(fb_height - pcmd->ClipRect.w), (int)(pcmd->ClipRect.z - pcmd->ClipRect.x), (int)(pcmd->ClipRect.w - pcmd->ClipRect.y));
                        glDrawElements(GL_TRIANGLES, (GLsizei)pcmd->ElemCount, sizeof(ImDrawIdx) == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT, idx_buffer_offset);
                    }
                    idx_buffer_offset += pcmd->ElemCount;
                }
            }
            
            // Restore modified GL state
            glUseProgram(last_program);
            glBindTexture(GL_TEXTURE_2D, last_texture);
            glBindVertexArray(last_vertex_array);
            glBindBuffer(GL_ARRAY_BUFFER, last_array_buffer);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, last_element_array_buffer);
            glBlendEquationSeparate(last_blend_equation_rgb, last_blend_equation_alpha);
            glBlendFunc(last_blend_src, last_blend_dst);
            if (last_enable_blend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
            if (last_enable_cull_face) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
            if (last_enable_depth_test) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
            if (last_enable_scissor_test) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
            glViewport(last_viewport[0], last_viewport[1], (GLsizei)last_viewport[2], (GLsizei)last_viewport[3]);
        };
        
        io.SetClipboardTextFn = [](const char * txt)
        {
            ImGuiApp & s = ImGuiApp::get_instance();
            glfwSetClipboardString(s.window, txt);
        };
        
        io.GetClipboardTextFn = []() -> const char *
        {
            ImGuiApp & s = ImGuiApp::get_instance();
            return glfwGetClipboardString(s.window);
        };
        
        #ifdef _WIN32
        //io.ImeWindowHandle = glfwGetWin32Window(state.window);
        #endif
    }
    
    ImGuiManager::~ImGuiManager()
    {
        destroy_render_objects();
        ImGui::Shutdown();
    }
    
    void ImGuiManager::update_input(const avl::InputEvent & e)
    {
        ImGuiApp & state = ImGuiApp::get_instance();
        ImGuiIO & io = ImGui::GetIO();
        
        if (e.type == InputEvent::Type::MOUSE)
        {
            if (e.action == GLFW_PRESS)
            {
                int button = clamp<int>(e.value[0], 0, 3);
                state.MousePressed[button] = true;
            }
        }
        
        if (e.type == InputEvent::Type::SCROLL)
        {
            state.MouseWheel += (float)e.value[1]; // Use fractional mouse wheel, 1.0 unit 5 lines.
        }
        
        if (e.type == InputEvent::Type::KEY)
        {
            if (e.action == GLFW_PRESS)
                io.KeysDown[e.value[0]] = true;
            if (e.action == GLFW_RELEASE)
                io.KeysDown[e.value[0]] = false;
            
            io.KeyCtrl = io.KeysDown[GLFW_KEY_LEFT_CONTROL] || io.KeysDown[GLFW_KEY_RIGHT_CONTROL];
            io.KeyShift = io.KeysDown[GLFW_KEY_LEFT_SHIFT] || io.KeysDown[GLFW_KEY_RIGHT_SHIFT];
            io.KeyAlt = io.KeysDown[GLFW_KEY_LEFT_ALT] || io.KeysDown[GLFW_KEY_RIGHT_ALT];
        }
        
        if (e.type == InputEvent::Type::CHAR)
        {
            ImGuiIO & io = ImGui::GetIO();
            
            if (e.value[0] > 0 && e.value[0] < 0x10000)
            {
                io.AddInputCharacter((unsigned short)e.value[0]);
            }
        }

    }

    bool ImGuiManager::create_fonts_texture()
    {
        ImGuiApp & state = ImGuiApp::get_instance();
        
        // Build texture atlas
        ImGuiIO& io = ImGui::GetIO();
        unsigned char* pixels;
        int width, height;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);   // Load as RGBA 32-bits for OpenGL3 demo because it is more likely to be compatible with user's existing shader.
        
        // Upload texture to graphics system
        GLint last_texture;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
        glGenTextures(1, &state.FontTexture);
        glBindTexture(GL_TEXTURE_2D, state.FontTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        
        // Store our identifier
        io.Fonts->TexID = (void *)(intptr_t)state.FontTexture;
        
        // Restore state
        glBindTexture(GL_TEXTURE_2D, last_texture);
        
        return true;
    }
    
    bool ImGuiManager::create_render_objects()
    {
        ImGuiApp & state = ImGuiApp::get_instance();
        
        // Backup GL state
        GLint last_texture, last_array_buffer, last_vertex_array;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &last_array_buffer);
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &last_vertex_array);
        
        const GLchar *vertex_shader =
        "#version 330\n"
        "uniform mat4 ProjMtx;\n"
        "in vec2 Position;\n"
        "in vec2 UV;\n"
        "in vec4 Color;\n"
        "out vec2 Frag_UV;\n"
        "out vec4 Frag_Color;\n"
        "void main()\n"
        "{\n"
        "	Frag_UV = UV;\n"
        "	Frag_Color = Color;\n"
        "	gl_Position = ProjMtx * vec4(Position.xy,0,1);\n"
        "}\n";
        
        const GLchar* fragment_shader =
        "#version 330\n"
        "uniform sampler2D Texture;\n"
        "in vec2 Frag_UV;\n"
        "in vec4 Frag_Color;\n"
        "out vec4 Out_Color;\n"
        "void main()\n"
        "{\n"
        "	Out_Color = Frag_Color * texture( Texture, Frag_UV.st);\n"
        "}\n";
        
        state.ShaderHandle = glCreateProgram();
        state.VertHandle = glCreateShader(GL_VERTEX_SHADER);
        state.FragHandle = glCreateShader(GL_FRAGMENT_SHADER);
        
        glShaderSource(state.VertHandle, 1, &vertex_shader, 0);
        glShaderSource(state.FragHandle, 1, &fragment_shader, 0);
        glCompileShader(state.VertHandle);
        glCompileShader(state.FragHandle);
        glAttachShader(state.ShaderHandle, state.VertHandle);
        glAttachShader(state.ShaderHandle, state.FragHandle);
        glLinkProgram(state.ShaderHandle);
        
        state.AttribLocationTex = glGetUniformLocation(state.ShaderHandle, "Texture");
        state.AttribLocationProjMtx = glGetUniformLocation(state.ShaderHandle, "ProjMtx");
        state.AttribLocationPosition = glGetAttribLocation(state.ShaderHandle, "Position");
        state.AttribLocationUV = glGetAttribLocation(state.ShaderHandle, "UV");
        state.AttribLocationColor = glGetAttribLocation(state.ShaderHandle, "Color");
        
        glGenBuffers(1, &state.VboHandle);
        glGenBuffers(1, &state.ElementsHandle);
        
        glGenVertexArrays(1, &state.VaoHandle);
        glBindVertexArray(state.VaoHandle);
        glBindBuffer(GL_ARRAY_BUFFER, state.VboHandle);
        glEnableVertexAttribArray(state.AttribLocationPosition);
        glEnableVertexAttribArray(state.AttribLocationUV);
        glEnableVertexAttribArray(state.AttribLocationColor);
        
        #define OFFSETOF(TYPE, ELEMENT) ((size_t)&(((TYPE *)0)->ELEMENT))
        glVertexAttribPointer(state.AttribLocationPosition, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), (GLvoid*)OFFSETOF(ImDrawVert, pos));
        glVertexAttribPointer(state.AttribLocationUV, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), (GLvoid*)OFFSETOF(ImDrawVert, uv));
        glVertexAttribPointer(state.AttribLocationColor, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(ImDrawVert), (GLvoid*)OFFSETOF(ImDrawVert, col));
        #undef OFFSETOF
        
        create_fonts_texture();
        
        // Restore modified GL state
        glBindTexture(GL_TEXTURE_2D, last_texture);
        glBindBuffer(GL_ARRAY_BUFFER, last_array_buffer);
        glBindVertexArray(last_vertex_array);
        
        return true;
    }
    
    void ImGuiManager::destroy_render_objects()
    {
        ImGuiApp & state = ImGuiApp::get_instance();
        
        if (state.VaoHandle) glDeleteVertexArrays(1, &state.VaoHandle);
        if (state.VboHandle) glDeleteBuffers(1, &state.VboHandle);
        if (state.ElementsHandle) glDeleteBuffers(1, &state.ElementsHandle);
        state.VaoHandle = state.VboHandle = state.ElementsHandle = 0;
        
        glDetachShader(state.ShaderHandle, state.VertHandle);
        glDeleteShader(state.VertHandle);
        state.VertHandle = 0;
        
        glDetachShader(state.ShaderHandle, state.FragHandle);
        glDeleteShader(state.FragHandle);
        state.FragHandle = 0;
        
        glDeleteProgram(state.ShaderHandle);
        state.ShaderHandle = 0;
        
        if (state.FontTexture)
        {
            glDeleteTextures(1, &state.FontTexture);
            ImGui::GetIO().Fonts->TexID = 0;
            state.FontTexture = 0;
        }
    }
    
    void ImGuiManager::begin_frame()
    {
        ImGuiApp & state = ImGuiApp::get_instance();
        
        if (!state.FontTexture)
            create_render_objects();
        
        ImGuiIO & io = ImGui::GetIO();
        
        // Setup display size (every frame to accommodate for window resizing)
        int w, h;
        int display_w, display_h;
        glfwGetWindowSize(state.window, &w, &h);
        glfwGetFramebufferSize(state.window, &display_w, &display_h);
        io.DisplaySize = ImVec2((float)w, (float)h);
        io.DisplayFramebufferScale = ImVec2(w > 0 ? ((float)display_w / w) : 0, h > 0 ? ((float)display_h / h) : 0);
        
        // Setup time step
        double current_time =  glfwGetTime();
        io.DeltaTime = state.Time > 0.0 ? (float)(current_time - state.Time) : (float)(1.0f/60.0f);
        state.Time = current_time;
        
        // Setup inputs
        // (we already got mouse wheel, keyboard keys & characters from glfw callbacks polled in glfwPollEvents())
        if (glfwGetWindowAttrib(state.window, GLFW_FOCUSED))
        {
            double mouse_x, mouse_y;
            glfwGetCursorPos(state.window, &mouse_x, &mouse_y);
            io.MousePos = ImVec2((float)mouse_x, (float)mouse_y);
        }
        else
        {
            io.MousePos = ImVec2(-1,-1);
        }
        
        for (int i = 0; i < 3; i++)
        {
            // If a mouse press event came, always pass it as "mouse held this frame", so we don't miss click-release events that are shorter than 1 frame
            io.MouseDown[i] = state.MousePressed[i] || glfwGetMouseButton(state.window, i) != 0;
            state.MousePressed[i] = false;
        }
        
        io.MouseWheel = state.MouseWheel;
        state.MouseWheel = 0.0f;
        
        // Hide OS mouse cursor if ImGui is drawing it
        glfwSetInputMode(state.window, GLFW_CURSOR, io.MouseDrawCursor ? GLFW_CURSOR_HIDDEN : GLFW_CURSOR_NORMAL);
        
        // Start the frame
        ImGui::NewFrame();
    }
    
    void ImGuiManager::end_frame()
    {
        ImGui::Render();
    }

    //////////////////////////////
    //   Helper Functionality   //
    //////////////////////////////

    void Image(const GlTexture & texture, const ImVec2 & size, const ImVec2 & uv0, const ImVec2 & uv1, const ImVec4 & tint_col, const ImVec4 & border_col)
    {
        ImGui::Image((void *)(intptr_t) texture.get_gl_handle(), size, uv0, uv1, tint_col, border_col);
    }

    bool ImageButton(const GlTexture & texture, const ImVec2 & size, const ImVec2 & uv0, const ImVec2 & uv1, int frame_padding, const ImVec4 & bg_col, const ImVec4 & tint_col)
    {
        return ImGui::ImageButton((void *)(intptr_t) texture.get_gl_handle(), size, uv0, uv1, frame_padding, bg_col, tint_col);
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
        bool result = ImGui::ListBox(label, current_item, names.data(), (int) names.size(), height_in_items);
        for (auto &name : names) delete [] name;
        return result;
    }

    bool InputText(const char* label, std::string* buf, ImGuiInputTextFlags flags, ImGuiTextEditCallback callback, void * user_data)
    {
        char *buffer = new char[buf->size()+128];
        std::strcpy(buffer, buf->c_str());
        bool result = ImGui::InputText(label, buffer, buf->size()+128, flags, callback, user_data);
        if (result) *buf = std::string(buffer);
        delete [] buffer;
        return result;
    }

    bool InputTextMultiline(const char* label, std::string* buf, const ImVec2 & size, ImGuiInputTextFlags flags, ImGuiTextEditCallback callback, void * user_data)
    {
        char *buffer = new char[buf->size()+128];
        std::strcpy(buffer, buf->c_str());
        bool result = ImGui::InputTextMultiline(label, buffer, buf->size()+128, size, flags, callback, user_data);
        if (result) *buf = std::string(buffer);
        delete [] buffer;
        return result;
    }

    bool Combo(const char* label, int * current_item, const std::vector<std::string> & items, int height_in_items)
    {
        std::string itemsNames;
        for (auto item : items) itemsNames += item + '\0';
        itemsNames += '\0';
        std::vector<char> charArray(itemsNames.begin(), itemsNames.end());
        bool result = ImGui::Combo(label, current_item, (const char*) &charArray[0], height_in_items);
        return result;
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

    ScopedMainMenuBar::ScopedMainMenuBar() { opened = ImGui::BeginMainMenuBar(); }

    ScopedMainMenuBar::~ScopedMainMenuBar() { if (opened) ImGui::EndMainMenuBar(); }

    ScopedMenuBar::ScopedMenuBar() { opened = ImGui::BeginMenuBar(); }

    ScopedMenuBar::~ScopedMenuBar() { if (opened) ImGui::EndMenuBar(); }

}