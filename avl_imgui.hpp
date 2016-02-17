#pragma once

#ifndef AVL_IMGUI_H
#define AVL_IMGUI_H

#include <memory>
#include <vector>
#include <map>

#include "glfw_app.hpp"

using namespace avl;

#include "imgui/imgui.h"

// Implicit casts for linalg types

#define IM_VEC2_CLASS_EXTRA                                        \
ImVec2(const float2 & f) { x = f.x; y = f.y; }                     \
operator float2() const { return float2(x,y); }                    \
ImVec2(const int2 & f) { x = f.x; y = f.y; }                       \
operator int2() const { return int2(x,y); }

#define IM_VEC4_CLASS_EXTRA                                        \
ImVec4(const float4 & f) { x = f.x; y = f.y; z = f.z; w = f.w; }   \
operator float4() const { return float4(x,y,z,w); } 

namespace ImGui {
    
    class Options
    {

        bool mAutoRender = true;

        ImGuiStyle mStyle;
        std::vector<std::pair<std::string,float>>  mFonts;
        std::map<std::string,std::vector<ImWchar>>  mFontsGlyphRanges;

        std::shared_ptr<GLFWwindow> glfwWindowHandle;

    public:

        Options(const std::shared_ptr<GLFWwindow> & window);

        // species whether the block should call ImGui::NewFrame and ImGui::Render automatically. Default to true.
        Options & autoRender(bool autoRender);
        
        // sets the font to use in ImGui
        Options & font(const std::string &fontPath, float size);
        
        // sets the list of available fonts to use in ImGui
        Options & fonts(const std::vector<std::pair<std::string,float>> &fontPaths);
        
        // sets the font to use in ImGui
        Options & fontGlyphRanges(const std::string &name, const std::vector<ImWchar> &glyphRanges);
        
        // Global alpha applies to everything in ImGui
        Options & alpha(float a);
        
        // Padding within a window
        Options & windowPadding(const float2 &padding);
        
        // Minimum window size
        Options & windowMinSize(const float2 &minSize);
        
        // Radius of window corners rounding. Set to 0.0f to have rectangular windows
        Options & windowRounding(float rounding);
        
        // Alignment for title bar text
        Options & windowTitleAlign(ImGuiAlign align);
        
        // Radius of child window corners rounding. Set to 0.0f to have rectangular windows
        Options & childWindowRounding(float rounding);
        
        // Padding within a framed rectangle (used by most widgets)
        Options & framePadding(const float2 &padding);
        
        // Radius of frame corners rounding. Set to 0.0f to have rectangular frame (used by most widgets).
        Options & frameRounding(float rounding);
        
        // Horizontal and vertical spacing between widgets/lines
        Options & itemSpacing(const float2 &spacing);
        
        // Horizontal and vertical spacing between within elements of a composed widget (e.g. a slider and its label)
        Options & itemInnerSpacing(const float2 &spacing);
        
        // Expand bounding box for touch-based system where touch position is not accurate enough (unnecessary for mouse inputs).
        Options & touchExtraPadding(const float2 &padding);
        
        // Default alpha of window background, if not specified in ImGui::Begin()
        Options & windowFillAlphaDefault(float defaultAlpha);
        
        // Horizontal spacing when entering a tree node
        Options & indentSpacing(float spacing);
        
        // Minimum horizontal spacing between two columns
        Options & columnsMinSpacing(float minSpacing);
        
        // Width of the vertical scroll bar, Height of the horizontal scrollbar
        Options & scrollBarSize(float size);
        
        // Radius of grab corners for scrollbar
        Options & scrollbarRounding(float rounding);
        
        // Minimum width/height of a grab box for slider/scrollbar
        Options & grabMinSize(float minSize);
        
        // Radius of grabs corners rounding. Set to 0.0f to have rectangular slider grabs.
        Options & grabRounding(float rounding);
        
        // Window positions are clamped to be visible within the display area by at least this amount. Only covers regular windows.
        Options & displayWindowPadding(const float2 &padding);
        
        // If you cannot see the edge of your screen (e.g. on a TV) increase the safe area padding. Covers popups/tooltips as well regular windows.
        Options & displaySafeAreaPadding(const float2 &padding);
        
        // Enable anti-aliasing on lines/borders. Disable if you are really tight on CPU/GPU.
        Options & antiAliasedLines(bool antiAliasing);
        
        // Enable anti-aliasing on filled shapes (rounded rectangles, circles, etc.)
        Options & antiAliasedShapes(bool antiAliasing);
        
        // Tessellation tolerance. Decrease for highly tessellated curves (higher quality, more polygons), increase to reduce quality.
        Options & curveTessellationTol(float tessTolerance);
        
        Options & defaultTheme();

        Options & build_dark_theme();
        
        // sets theme colors
        Options & color(ImGuiCol option, const float4 & color);
        
        // returns whether the block should call ImGui::NewFrame and ImGui::Render automatically
        bool isAutoRenderEnabled() const { return mAutoRender; }
        
        // returns the window that will be use to connect the signals and render ImGui
        std::shared_ptr<GLFWwindow> getWindow() const { return glfwWindowHandle; }
        
        // returns the list of available fonts to use in ImGui
        const std::vector<std::pair<std::string,float>>& getFonts() const { return mFonts; }
        
        // returns the glyph ranges if available for this font
        const ImWchar* getFontGlyphRanges(const std::string &name) const;
        
        // returns the window that will be use to connect the signals and render ImGui
        const ImGuiStyle& getStyle() const { return mStyle; }
    };
    
    //////////////////////////////
    //   Helper Functionality   //
    //////////////////////////////

    void Image(const GlTexture & texture, const ImVec2& size, const ImVec2& uv0 = ImVec2(0,1), const ImVec2& uv1 = ImVec2(1,0), const ImVec4& tint_col = ImVec4(1,1,1,1), const ImVec4& border_col = ImVec4(0,0,0,0));
    bool ImageButton(const GlTexture & texture, const ImVec2& size, const ImVec2& uv0 = ImVec2(0,1),  const ImVec2& uv1 = ImVec2(1,0), int frame_padding = -1, const ImVec4& bg_col = ImVec4(0,0,0,1), const ImVec4& tint_col = ImVec4(1,1,1,1));
    void PushFont(const std::string& name = "");
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
    
    struct ScopedFont : Noncopyable
    {
        ScopedFont(ImFont* font);
        ScopedFont(const std::string &name);
        ~ScopedFont();
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
    protected:
        bool mOpened;
    };
    
    struct ScopedMenuBar : Noncopyable 
    {
        ScopedMenuBar();
        ~ScopedMenuBar();
    protected:
        bool mOpened;
    };
    
}

#endif // AVL_IMGUI_H