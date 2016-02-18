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
private:
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
        void setup(GLFWwindow * win);
        
        void update_input_mouse(int button, int action, int /*mods*/);
        void update_input_scroll(double /*xoffset*/, double yoffset);
        void update_input_key(int key, int, int action, int mods);
        void update_input_char(unsigned int c);

        bool create_fonts_texture();
        bool create_render_objects();
        void destroy_render_objects();
        void new_frame();
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