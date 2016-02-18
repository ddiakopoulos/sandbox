#pragma once

#ifndef AVL_IMGUI_H
#define AVL_IMGUI_H

#include <memory>
#include <vector>
#include <map>

#include "glfw_app.hpp"
#include "GL_API.hpp"

using namespace avl;

// Implicit casts for linalg types

#define IM_VEC2_CLASS_EXTRA                                        \
ImVec2(const float2 & f) { x = f.x; y = f.y; }                     \
operator float2() const { return float2(x,y); }                    \
ImVec2(const int2 & f) { x = f.x; y = f.y; }                       \
operator int2() const { return int2(x,y); }

#define IM_VEC4_CLASS_EXTRA                                        \
ImVec4(const float4 & f) { x = f.x; y = f.y; z = f.z; w = f.w; }   \
operator float4() const { return float4(x,y,z,w); } 

#include "imgui/imgui.h"

template <typename T>
class Singleton : Noncopyable
{
    Singleton(const Singleton<T> &);
    Singleton & operator = (const Singleton<T> &);
protected:
    static T * single;
    Singleton() = default;
    ~Singleton() = default;
public:
    static T & get_instance() { return *single; };
};

struct ImGuiApp : public Singleton<ImGuiApp>
{
    GLFWwindow * window;
    double       Time = 0.0f;
    bool         MousePressed[3] = { false, false, false };
    float        MouseWheel = 0.0f;
    GLuint       FontTexture = 0;
    int          ShaderHandle = 0, VertHandle = 0, FragHandle = 0;
    int          AttribLocationTex = 0, AttribLocationProjMtx = 0;
    int          AttribLocationPosition = 0, AttribLocationUV = 0, AttribLocationColor = 0;
    unsigned int VboHandle = 0, VaoHandle = 0, ElementsHandle = 0;
    friend class Singleton<ImGuiApp>;
};

namespace ImGui
{
    struct ImGuiManager
    {
        
        void setup(GLFWwindow * win)
        {
            ImGuiApp & state = ImGuiApp::get_instance();
            
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
            io.ImeWindowHandle = glfwGetWin32Window(state.window);
            #endif
        }
        
        void update_input_mouse(int button, int action, int /*mods*/)
        {
            ImGuiApp & state = ImGuiApp::get_instance();
            if (action == GLFW_PRESS && button >= 0 && button < 3)
                state.MousePressed[button] = true;
        }
        
        void update_input_scroll(double /*xoffset*/, double yoffset)
        {
            ImGuiApp & state = ImGuiApp::get_instance();
            state.MouseWheel += (float)yoffset; // Use fractional mouse wheel, 1.0 unit 5 lines.
        }
        
        void update_input_key(int key, int, int action, int mods)
        {
            ImGuiIO & io = ImGui::GetIO();
            if (action == GLFW_PRESS)
                io.KeysDown[key] = true;
            if (action == GLFW_RELEASE)
                io.KeysDown[key] = false;
            
            (void) mods; // Modifiers are not reliable across systems
            io.KeyCtrl = io.KeysDown[GLFW_KEY_LEFT_CONTROL] || io.KeysDown[GLFW_KEY_RIGHT_CONTROL];
            io.KeyShift = io.KeysDown[GLFW_KEY_LEFT_SHIFT] || io.KeysDown[GLFW_KEY_RIGHT_SHIFT];
            io.KeyAlt = io.KeysDown[GLFW_KEY_LEFT_ALT] || io.KeysDown[GLFW_KEY_RIGHT_ALT];
        }
        
        void update_input_char(unsigned int c)
        {
            ImGuiIO & io = ImGui::GetIO();
            if (c > 0 && c < 0x10000)  io.AddInputCharacter((unsigned short)c);
        }

        
        bool create_fonts_texture()
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
        
        bool create_render_objects()
        {
            ImGuiApp & state = ImGuiApp::get_instance();
            
            // Backup GL state
            GLint last_texture, last_array_buffer, last_vertex_array;
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
            glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &last_array_buffer);
            glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &last_vertex_array);
            
            constexpr const char * imguiVertShader = R"(#version 330
                uniform mat4 ProjMtx;
                in vec2 Position;
                in vec2 UV;
                in vec4 Color;
                out vec2 Frag_UV;
                out vec4 Frag_Color;
                void main()
                {
                    Frag_UV = UV;
                    Frag_Color = Color;
                    gl_Position = ProjMtx * vec4(Position.xy,0,1);
                }
            )";
            
            constexpr const char * imguiFragShader = R"(#version 330
                uniform sampler2D Texture;
                in vec2 Frag_UV;
                in vec4 Frag_Color;
                out vec4 Out_Color;
                void main()
                {
                    Out_Color = Frag_Color * texture(Texture, Frag_UV.st);
                };
            )";
            
            state.ShaderHandle = glCreateProgram();
            state.VertHandle = glCreateShader(GL_VERTEX_SHADER);
            state.FragHandle = glCreateShader(GL_FRAGMENT_SHADER);
            
            glShaderSource(state.VertHandle, 1, &imguiVertShader, 0);
            glShaderSource(state.FragHandle, 1, &imguiFragShader, 0);
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
        
        void destroy_render_objects()
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
        
        void new_frame()
        {
            ImGuiApp & state = ImGuiApp::get_instance();
            
            if (!state.FontTexture) create_render_objects();
            
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
        
    };
    
    inline ImGuiStyle make_dark_theme()
    {
        ImGuiStyle s = ImGuiStyle();
        
        s.WindowMinSize                          = ImVec2(160, 20);
        s.FramePadding                           = ImVec2(4, 2);
        s.ItemSpacing                            = ImVec2(6, 2);
        s.ItemInnerSpacing                       = ImVec2(6, 4);
        s.Alpha                                  = 0.95f;
        s.WindowFillAlphaDefault                 = 1.0f;
        s.WindowRounding                         = 4.0f;
        s.FrameRounding                          = 2.0f;
        s.IndentSpacing                          = 6.0f;
        s.ColumnsMinSpacing                      = 50.0f;
        s.GrabMinSize                            = 14.0f;
        s.GrabRounding                           = 16.0f;
        s.ScrollbarSize                          = 12.0f;
        s.ScrollbarRounding                      = 16.0f;
        
        s.Colors[ImGuiCol_Text]                  = ImVec4(0.86f, 0.93f, 0.89f, 0.61f);
        s.Colors[ImGuiCol_TextDisabled]          = ImVec4(0.86f, 0.93f, 0.89f, 0.28f);
        s.Colors[ImGuiCol_WindowBg]              = ImVec4(0.13f, 0.14f, 0.17f, 1.00f);
        s.Colors[ImGuiCol_ChildWindowBg]         = ImVec4(0.20f, 0.22f, 0.27f, 0.58f);
        s.Colors[ImGuiCol_Border]                = ImVec4(0.31f, 0.31f, 1.00f, 0.00f);
        s.Colors[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        s.Colors[ImGuiCol_FrameBg]               = ImVec4(0.20f, 0.22f, 0.27f, 1.00f);
        s.Colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.92f, 0.18f, 0.29f, 0.78f);
        s.Colors[ImGuiCol_FrameBgActive]         = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
        s.Colors[ImGuiCol_TitleBg]               = ImVec4(0.20f, 0.22f, 0.27f, 1.00f);
        s.Colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.20f, 0.22f, 0.27f, 0.75f);
        s.Colors[ImGuiCol_TitleBgActive]         = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
        s.Colors[ImGuiCol_MenuBarBg]             = ImVec4(0.20f, 0.22f, 0.27f, 0.47f);
        s.Colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.20f, 0.22f, 0.27f, 1.00f);
        s.Colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.47f, 0.77f, 0.83f, 0.21f);
        s.Colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.92f, 0.18f, 0.29f, 0.78f);
        s.Colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
        s.Colors[ImGuiCol_ComboBg]               = ImVec4(0.20f, 0.22f, 0.27f, 1.00f);
        s.Colors[ImGuiCol_CheckMark]             = ImVec4(0.71f, 0.22f, 0.27f, 1.00f);
        s.Colors[ImGuiCol_SliderGrab]            = ImVec4(0.47f, 0.77f, 0.83f, 0.14f);
        s.Colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
        s.Colors[ImGuiCol_Button]                = ImVec4(0.47f, 0.77f, 0.83f, 0.14f);
        s.Colors[ImGuiCol_ButtonHovered]         = ImVec4(0.92f, 0.18f, 0.29f, 0.86f);
        s.Colors[ImGuiCol_ButtonActive]          = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
        s.Colors[ImGuiCol_Header]                = ImVec4(0.92f, 0.18f, 0.29f, 0.76f);
        s.Colors[ImGuiCol_HeaderHovered]         = ImVec4(0.92f, 0.18f, 0.29f, 0.86f);
        s.Colors[ImGuiCol_HeaderActive]          = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
        s.Colors[ImGuiCol_Column]                = ImVec4(0.47f, 0.77f, 0.83f, 0.32f);
        s.Colors[ImGuiCol_ColumnHovered]         = ImVec4(0.92f, 0.18f, 0.29f, 0.78f);
        s.Colors[ImGuiCol_ColumnActive]          = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
        s.Colors[ImGuiCol_ResizeGrip]            = ImVec4(0.47f, 0.77f, 0.83f, 0.04f);
        s.Colors[ImGuiCol_ResizeGripHovered]     = ImVec4(0.92f, 0.18f, 0.29f, 0.78f);
        s.Colors[ImGuiCol_ResizeGripActive]      = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
        s.Colors[ImGuiCol_CloseButton]           = ImVec4(0.86f, 0.93f, 0.89f, 0.16f);
        s.Colors[ImGuiCol_CloseButtonHovered]    = ImVec4(0.86f, 0.93f, 0.89f, 0.39f);
        s.Colors[ImGuiCol_CloseButtonActive]     = ImVec4(0.86f, 0.93f, 0.89f, 1.00f);
        s.Colors[ImGuiCol_PlotLines]             = ImVec4(0.86f, 0.93f, 0.89f, 0.63f);
        s.Colors[ImGuiCol_PlotLinesHovered]      = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
        s.Colors[ImGuiCol_PlotHistogram]         = ImVec4(0.86f, 0.93f, 0.89f, 0.63f);
        s.Colors[ImGuiCol_PlotHistogramHovered]  = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
        s.Colors[ImGuiCol_TextSelectedBg]        = ImVec4(0.92f, 0.18f, 0.29f, 0.43f);
        s.Colors[ImGuiCol_TooltipBg]             = ImVec4(0.47f, 0.77f, 0.83f, 0.72f);
        s.Colors[ImGuiCol_ModalWindowDarkening]  = ImVec4(0.20f, 0.22f, 0.27f, 0.73f);
        
        return s;
    }
    
    //////////////////////////////
    //   Helper Functionality   //
    //////////////////////////////

    void Image(const GlTexture & texture, const ImVec2 & size, const ImVec2 & uv0 = ImVec2(0,1), const ImVec2& uv1 = ImVec2(1,0), const ImVec4& tint_col = ImVec4(1,1,1,1), const ImVec4& border_col = ImVec4(0,0,0,0));
    bool ImageButton(const GlTexture & texture, const ImVec2 & size, const ImVec2 & uv0 = ImVec2(0,1),  const ImVec2& uv1 = ImVec2(1,0), int frame_padding = -1, const ImVec4& bg_col = ImVec4(0,0,0,1), const ImVec4& tint_col = ImVec4(1,1,1,1));
    bool ListBox(const char* label, int* current_item, const std::vector<std::string>& items, int height_in_items = -1);
    bool InputText(const char* label, std::string* buf, ImGuiInputTextFlags flags = 0, ImGuiTextEditCallback callback = NULL, void* user_data = NULL);
    bool InputTextMultiline(const char* label, std::string* buf, const ImVec2& size = ImVec2(0,0), ImGuiInputTextFlags flags = 0, ImGuiTextEditCallback callback = NULL, void* user_data = NULL);
    bool Combo(const char* label, int* current_item, const std::vector<std::string>& items, int height_in_items = -1);
    
    ////////////////////////////////
    //   Scoped ImGui Utilities   //
    ////////////////////////////////

    struct ScopedWindow : Noncopyable
    {
        ScopedWindow(const std::string &name = "Debug", ImGuiWindowFlags flags = 0);
        ScopedWindow(const std::string &name, float2 size, float fillAlpha = -1.0f, ImGuiWindowFlags flags = 0);
        ~ScopedWindow();
    };
    
    struct ScopedChild : Noncopyable
    {
        ScopedChild(const std::string &name, float2 size = float2(0.f, 0.f), bool border = false, ImGuiWindowFlags extraFlags = 0);
        ~ScopedChild();
    };
    
    struct ScopedGroup : Noncopyable
    {
        ScopedGroup();
        ~ScopedGroup();
    };

    struct ScopedStyleColor : Noncopyable
    {
        ScopedStyleColor(ImGuiCol idx, const ImVec4& col);
        ~ScopedStyleColor();
    };
    
    struct ScopedStyleVar : Noncopyable
    {
        ScopedStyleVar(ImGuiStyleVar idx, float val);
        ScopedStyleVar(ImGuiStyleVar idx, const ImVec2 &val);
        ~ScopedStyleVar();
    };
    
    struct ScopedItemWidth : Noncopyable
    {
        ScopedItemWidth(float itemWidth);
        ~ScopedItemWidth();
    };
    
    struct ScopedTextWrapPos : Noncopyable
    {
        ScopedTextWrapPos(float wrapPosX = 0.0f);
        ~ScopedTextWrapPos();
    };
    
    struct ScopedId : Noncopyable
    {
        ScopedId(const std::string &name);
        ScopedId(const void *ptrId);
        ScopedId(const int intId);
        ~ScopedId();
    };
    
    struct ScopedMainMenuBar : Noncopyable
    {
        ScopedMainMenuBar();
        ~ScopedMainMenuBar();
        bool opened;
    };
    
    struct ScopedMenuBar : Noncopyable 
    {
        ScopedMenuBar();
        ~ScopedMenuBar();
        bool opened;
    };
    
}

#endif // AVL_IMGUI_H