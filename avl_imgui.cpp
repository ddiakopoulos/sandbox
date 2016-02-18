#include <functional>
#include <memory>

#include "avl_imgui.hpp"
#include "imgui/imgui_internal.h"

namespace ImGui 
{
    
    ///////////////////////////////////////
    //   ImGui Renderer Implementation   //
    ///////////////////////////////////////
    
    // https://github.com/ocornut/imgui/tree/master/examples/opengl3_example
    
    //////////////////////////////
    //   Helper Functionality   //
    //////////////////////////////

    void Image(const GlTexture & texture, const ImVec2 & size, const ImVec2 & uv0, const ImVec2 & uv1, const ImVec4 & tint_col, const ImVec4 & border_col)
    {
        Image((void *)(intptr_t) texture.get_gl_handle(), size, uv0, uv1, tint_col, border_col);
    }

    bool ImageButton(const GlTexture & texture, const ImVec2 & size, const ImVec2 & uv0, const ImVec2 & uv1, int frame_padding, const ImVec4 & bg_col, const ImVec4 & tint_col)
    {
        return ImageButton((void *)(intptr_t) texture.get_gl_handle(), size, uv0, uv1, frame_padding, bg_col, tint_col);
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
        bool result = ListBox(label, current_item, names.data(), (int) names.size(), height_in_items);
        for (auto &name : names) delete [] name;
        return result;
    }

    bool InputText(const char* label, std::string* buf, ImGuiInputTextFlags flags, ImGuiTextEditCallback callback, void * user_data)
    {
        char *buffer = new char[buf->size()+128];
        std::strcpy(buffer, buf->c_str());
        bool result = InputText(label, buffer, buf->size()+128, flags, callback, user_data);
        if (result) *buf = std::string(buffer);
        delete [] buffer;
        return result;
    }

    bool InputTextMultiline(const char* label, std::string* buf, const ImVec2 & size, ImGuiInputTextFlags flags, ImGuiTextEditCallback callback, void * user_data)
    {
        char *buffer = new char[buf->size()+128];
        std::strcpy(buffer, buf->c_str());
        bool result = InputTextMultiline(label, buffer, buf->size()+128, size, flags, callback, user_data);
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
        bool result = Combo(label, current_item, (const char*) &charArray[0], height_in_items);
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