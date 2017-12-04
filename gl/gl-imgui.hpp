#pragma once

#ifndef AVL_IMGUI_H
#define AVL_IMGUI_H

#include <memory>
#include <map>
#include <string>

#include "math-common.hpp"
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

using namespace avl;

struct ui_rect
{
    int2 min, max;
    bool contains(const int2 & p) const { return all(gequal(p, min)) && all(less(p, max)); }
};

struct GlTexture2D;
namespace avl
{
    class GLFWApp;
    struct InputEvent;
}

struct GLFWwindow;
namespace gui
{

    struct ImGuiInstanceData
    {
        GLFWwindow      * window;
        ImGuiContext    * context;
        double          Time = 0.0f;
        bool            MousePressed[3] = { false, false, false };
        float           MouseWheel = 0.0f;
        int             ShaderHandle = 0, VertHandle = 0, FragHandle = 0;
        int             AttribLocationTex = 0, AttribLocationProjMtx = 0;
        int             AttribLocationPosition = 0, AttribLocationUV = 0, AttribLocationColor = 0;
        uint32_t        FontTexture = 0;
        unsigned int    VboHandle = 0, VaoHandle = 0, ElementsHandle = 0;
    };

    class ImGuiInstance
    {
        bool create_fonts_texture();
        bool create_render_objects();
        void destroy_render_objects();
        ImGuiInstanceData data;
    public:
        ImGuiInstance(GLFWwindow * win);
        ~ImGuiInstance();
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
        s.Alpha                                  = 0.90f;
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
        s.Colors[ImGuiCol_ModalWindowDarkening]  = ImVec4(0.20f, 0.22f, 0.27f, 0.73f);
    }

    inline void make_light_theme()
    {
        ImGuiStyle & s = ImGui::GetStyle();

        s.WindowMinSize = ImVec2(160, 20);
        s.FramePadding = ImVec2(4, 2);
        s.ItemSpacing = ImVec2(6, 2);
        s.ItemInnerSpacing = ImVec2(6, 4);
        s.Alpha = 0.80f;
        s.WindowRounding = 0.0f;
        s.FrameRounding = 0.0f;
        s.IndentSpacing = 4.0f;
        s.ColumnsMinSpacing = 50.0f;
        s.GrabMinSize = 14.0f;
        s.GrabRounding = 4.0f;
        s.ScrollbarSize = 16.0f;
        s.ScrollbarRounding = 2.0f;

        s.Colors[ImGuiCol_Text] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
        s.Colors[ImGuiCol_TextDisabled] = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
        s.Colors[ImGuiCol_WindowBg] = ImVec4(0.94f, 0.94f, 0.94f, 1.00f);
        s.Colors[ImGuiCol_ChildWindowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        s.Colors[ImGuiCol_Border] = ImVec4(0.00f, 0.00f, 0.00f, 0.39f);
        s.Colors[ImGuiCol_BorderShadow] = ImVec4(1.00f, 1.00f, 1.00f, 0.10f);
        s.Colors[ImGuiCol_FrameBg] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
        s.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.90f, 0.11f, 0.05f, 0.40f);
        s.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.90f, 0.11f, 0.05f, 0.90f);
        s.Colors[ImGuiCol_TitleBg] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
        s.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(1.00f, 1.00f, 1.00f, 0.51f);
        s.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.82f, 0.82f, 0.82f, 1.00f);
        s.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.86f, 0.86f, 0.86f, 1.00f);
        s.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.25f, 0.25f, 0.25f, 0.53f);
        s.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.69f, 0.69f, 0.69f, 0.80f);
        s.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.49f, 0.49f, 0.49f, 0.80f);
        s.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.49f, 0.49f, 0.49f, 1.00f);
        s.Colors[ImGuiCol_CheckMark] = ImVec4(0.90f, 0.11f, 0.05f, 1.00f);
        s.Colors[ImGuiCol_SliderGrab] = ImVec4(0.90f, 0.11f, 0.05f, 0.78f);
        s.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.90f, 0.11f, 0.05f, 1.00f);
        s.Colors[ImGuiCol_Button] = ImVec4(0.90f, 0.11f, 0.05f, 0.40f);
        s.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.90f, 0.11f, 0.05f, 1.00f);
        s.Colors[ImGuiCol_ButtonActive] = ImVec4(0.06f, 0.53f, 0.25f, 1.00f);
        s.Colors[ImGuiCol_Header] = ImVec4(0.90f, 0.11f, 0.05f, 0.8f);
        s.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.90f, 0.11f, 0.05f, 0.80f);
        s.Colors[ImGuiCol_HeaderActive] = ImVec4(0.90f, 0.11f, 0.05f, 1.00f);
        s.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.82f, 0.82f, 0.82f, 1.00f);
        s.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.90f, 0.11f, 0.05f, 0.67f);
        s.Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.90f, 0.11f, 0.05f, 0.95f);
        s.Colors[ImGuiCol_CloseButton] = ImVec4(0.60f, 0.60f, 0.60f, 0.50f);
        s.Colors[ImGuiCol_CloseButtonHovered] = ImVec4(0.25f, 0.39f, 0.36f, 1.00f);
        s.Colors[ImGuiCol_CloseButtonActive] = ImVec4(0.25f, 0.39f, 0.36f, 1.00f);
        s.Colors[ImGuiCol_PlotLines] = ImVec4(0.39f, 0.39f, 0.39f, 1.00f);
        s.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
        s.Colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
        s.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
        s.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.90f, 0.11f, 0.05f, 0.35f);
        s.Colors[ImGuiCol_ModalWindowDarkening] = ImVec4(0.20f, 0.20f, 0.20f, 0.35f);
    }
    
    //////////////////////////////
    //   Helper Functionality   //
    //////////////////////////////

    void Img(const int & texture, const char * label, const ImVec2 & size, const ImVec2 & uv0 = ImVec2(0,1), const ImVec2& uv1 = ImVec2(1,0), const ImVec4& tint_col = ImVec4(1,1,1,1), const ImVec4& border_col = ImVec4(0,0,0,0));
    bool ImageButton(const int & texture, const ImVec2 & size, const ImVec2 & uv0 = ImVec2(0,1),  const ImVec2& uv1 = ImVec2(1,0), int frame_padding = -1, const ImVec4& bg_col = ImVec4(0,0,0,1), const ImVec4& tint_col = ImVec4(1,1,1,1));
    bool ListBox(const char* label, int* current_item, const std::vector<std::string>& items, int height_in_items = -1);
    bool InputText(const char* label, std::string* buf, ImGuiInputTextFlags flags = 0, ImGuiTextEditCallback callback = NULL, void* user_data = NULL);
    bool InputTextMultiline(const char* label, std::string* buf, const ImVec2& size = ImVec2(0,0), ImGuiInputTextFlags flags = 0, ImGuiTextEditCallback callback = NULL, void* user_data = NULL);
    bool Combo(const char* label, int* current_item, const std::vector<std::string>& items, int height_in_items = -1);

    class imgui_menu_stack
    {
        bool * keys;
        int current_mods;
        std::vector<bool> open;
    public:
        imgui_menu_stack(const GLFWApp & app, bool * keys);
        void app_menu_begin();
        void begin(const char * label, bool enabled = true);
        bool item(const char * label, int mods = 0, int key = 0, bool enabled = true);
        void end();
        void app_menu_end();
    };

    inline void imgui_fixed_window_begin(const char * name, const ui_rect & r)
    {
        ImGui::SetNextWindowPos(r.min);
        ImGui::SetNextWindowSize(r.max - r.min);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(0, 0));
        bool result = ImGui::Begin(name, NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
        ImGui::TextColored({ 0, 0, 0,1 }, name);
        ImGui::Separator();
        assert(result);
    }

    inline void imgui_fixed_window_end()
    {
        ImGui::End();
        ImGui::PopStyleVar(2);
    }

}

#endif // AVL_IMGUI_H