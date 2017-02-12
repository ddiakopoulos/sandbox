#pragma once

#ifndef AVL_IMGUI_H
#define AVL_IMGUI_H

#include <memory>
#include <vector>
#include <map>

#include "linalg_util.hpp"
#include "util.hpp"

// Implicit casts for linalg types
#define IM_VEC2_CLASS_EXTRA                                             \
ImVec2(const avl::float2 & f) { x = f.x; y = f.y; }                     \
operator avl::float2() const { return avl::float2(x,y); }               \
ImVec2(const avl::int2 & f) { x = f.x; y = f.y; }                       \
operator avl::int2() const { return avl::int2(x,y); }

#define IM_VEC4_CLASS_EXTRA                                             \
ImVec4(const avl::float4 & f) { x = f.x; y = f.y; z = f.z; w = f.w; }   \
operator avl::float4() const { return avl::float4(x,y,z,w); }

#include "imgui/imgui.h"

class GlTexture2D;
namespace avl
{
    struct InputEvent;
}

struct GLFWwindow;
namespace gui
{

    struct ImGuiApp : public avl::Singleton<ImGuiApp>
    {
        GLFWwindow * window;
        double       Time = 0.0f;
        bool         MousePressed[3] = { false, false, false };
        float        MouseWheel = 0.0f;
        uint32_t     FontTexture = 0;
        int          ShaderHandle = 0, VertHandle = 0, FragHandle = 0;
        int          AttribLocationTex = 0, AttribLocationProjMtx = 0;
        int          AttribLocationPosition = 0, AttribLocationUV = 0, AttribLocationColor = 0;
        unsigned int VboHandle = 0, VaoHandle = 0, ElementsHandle = 0;
        friend class avl::Singleton<ImGuiApp>;
    };

    class ImGuiManager
    {
        bool create_fonts_texture();
        bool create_render_objects();
        void destroy_render_objects();
    public:
        
        ImGuiManager(GLFWwindow * win);
        ~ImGuiManager();
        
        void update_input(const avl::InputEvent & e);

        void begin_frame();
        void end_frame();
    };
    
    inline void make_dark_theme()
    {
        ImGuiStyle & s = ImGui::GetStyle();
        
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
    }
    
    //////////////////////////////
    //   Helper Functionality   //
    //////////////////////////////

    void Image(const GlTexture2D & texture, const ImVec2 & size, const ImVec2 & uv0 = ImVec2(0,1), const ImVec2& uv1 = ImVec2(1,0), const ImVec4& tint_col = ImVec4(1,1,1,1), const ImVec4& border_col = ImVec4(0,0,0,0));
    bool ImageButton(const GlTexture2D & texture, const ImVec2 & size, const ImVec2 & uv0 = ImVec2(0,1),  const ImVec2& uv1 = ImVec2(1,0), int frame_padding = -1, const ImVec4& bg_col = ImVec4(0,0,0,1), const ImVec4& tint_col = ImVec4(1,1,1,1));
    bool ListBox(const char* label, int* current_item, const std::vector<std::string>& items, int height_in_items = -1);
    bool InputText(const char* label, std::string* buf, ImGuiInputTextFlags flags = 0, ImGuiTextEditCallback callback = NULL, void* user_data = NULL);
    bool InputTextMultiline(const char* label, std::string* buf, const ImVec2& size = ImVec2(0,0), ImGuiInputTextFlags flags = 0, ImGuiTextEditCallback callback = NULL, void* user_data = NULL);
    bool Combo(const char* label, int* current_item, const std::vector<std::string>& items, int height_in_items = -1);
    
    ////////////////////////////////
    //   Scoped ImGui Utilities   //
    ////////////////////////////////

    struct ScopedWindow : avl::Noncopyable
    {
        ScopedWindow(const std::string &name = "Debug", ImGuiWindowFlags flags = 0);
        ScopedWindow(const std::string &name, avl::float2 size, float fillAlpha = -1.0f, ImGuiWindowFlags flags = 0);
        ~ScopedWindow();
    };
    
    struct ScopedChild : avl::Noncopyable
    {
        ScopedChild(const std::string &name, avl::float2 size = avl::float2(0.f, 0.f), bool border = false, ImGuiWindowFlags extraFlags = 0);
        ~ScopedChild();
    };
    
    struct ScopedGroup : avl::Noncopyable
    {
        ScopedGroup();
        ~ScopedGroup();
    };

    struct ScopedStyleColor : avl::Noncopyable
    {
        ScopedStyleColor(ImGuiCol idx, const ImVec4& col);
        ~ScopedStyleColor();
    };
    
    struct ScopedStyleVar : avl::Noncopyable
    {
        ScopedStyleVar(ImGuiStyleVar idx, float val);
        ScopedStyleVar(ImGuiStyleVar idx, const ImVec2 &val);
        ~ScopedStyleVar();
    };
    
    struct ScopedItemWidth : avl::Noncopyable
    {
        ScopedItemWidth(float itemWidth);
        ~ScopedItemWidth();
    };
    
    struct ScopedTextWrapPos : avl::Noncopyable
    {
        ScopedTextWrapPos(float wrapPosX = 0.0f);
        ~ScopedTextWrapPos();
    };
    
    struct ScopedId : avl::Noncopyable
    {
        ScopedId(const std::string &name);
        ScopedId(const void *ptrId);
        ScopedId(const int intId);
        ~ScopedId();
    };
    
    struct ScopedMainMenuBar : avl::Noncopyable
    {
        ScopedMainMenuBar();
        ~ScopedMainMenuBar();
        bool opened;
    };
    
    struct ScopedMenuBar : avl::Noncopyable
    {
        ScopedMenuBar();
        ~ScopedMenuBar();
        bool opened;
    };
    
}

#endif // AVL_IMGUI_H