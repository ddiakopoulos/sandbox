#pragma once

#ifndef imgui_utils_hpp
#define imgui_utils_hpp

#include "gl-api.hpp"
#include "uniforms.hpp"
#include "assets.hpp"
#include "material.hpp"
#include "gl-imgui.hpp"
#include "imgui/imgui_internal.h"

/////////////////////////////////////
//   ImGui Scene Editor Utilities  //
/////////////////////////////////////

namespace ImGui
{
    static auto vector_getter = [](void* vec, int idx, const char** out_text)
    {
        auto & vector = *static_cast<std::vector<std::string>*>(vec);
        if (idx < 0 || idx >= static_cast<int>(vector.size())) { return false; }
        *out_text = vector.at(idx).c_str();
        return true;
    };

    bool Combo(const char* label, int* currIndex, std::vector<std::string>& values)
    {
        if (values.empty()) { return false; }
        return Combo(label, currIndex, vector_getter, static_cast<void*>(&values), values.size());
    }

    bool ListBox(const char* label, int* currIndex, std::vector<std::string>& values)
    {
        if (values.empty()) { return false; }
        return ListBox(label, currIndex, vector_getter, static_cast<void*>(&values), values.size());
    }

    enum SplitType : uint32_t
    {
        Left,
        Right,
        Top,
        Bottom
    };

    typedef std::pair<Bounds2D, Bounds2D> SplitRegion;

    SplitRegion Split(const Bounds2D & r, int * v, SplitType t)
    {
        ImGuiWindow * window = ImGui::GetCurrentWindowRead();
        const ImGuiID id = window->GetID(v);
        const auto & io = ImGui::GetIO();

        float2 cursor = float2(io.MousePos);

        if (GImGui->ActiveId == id)
        {

            // Get the current mouse position relative to the desired axis
            if (io.MouseDown[0])
            {
                uint32_t position = 0;

                switch (t)
                {
                case Left:   position = cursor.x - r.min().x; break;
                case Right:  position = r.max().x - cursor.x; break;
                case Top:    position = cursor.y - r.min().y; break;
                case Bottom: position = r.max().y - cursor.y; break;
                }

                *v = position;
            }
            else ImGui::SetActiveID(0, nullptr);
        }

        SplitRegion result = { r,r };

        switch (t)
        {
        case Left:   result.first._min.x = (result.second._max.x = r.min().x + *v) + 8; break;
        case Right:  result.first._max.x = (result.second._min.x = r.max().x - *v) - 8; break;
        case Top:    result.first._min.y = (result.second._max.y = r.min().y + *v) + 8; break;
        case Bottom: result.first._max.y = (result.second._min.y = r.max().y - *v) - 8; break;
        }

        if (r.contains(cursor) && !result.first.contains(cursor) && !result.second.contains(cursor))
        {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Move);
            if (io.MouseClicked[0])
            {
                ImGui::SetActiveID(id, window);
            }
        }
        return result;
    };

}

template<class T> const T * query_metadata(const T & meta) { return &meta; }
template<class T> const T * query_metadata() { return nullptr; }
template<class T> struct range_metadata { T min, max; };
struct editor_hidden {  };

inline bool Edit(const char * label, std::string & s)
{
    char buffer[2048];
    assert(s.size() + 1 < sizeof(buffer));
    std::memcpy(buffer, s.data(), std::min(s.size() + 1, sizeof(buffer)));
    if (ImGui::InputText(label, buffer, sizeof(buffer)))
    {
        s = buffer;
        return true;
    }
    else return false;
}

inline bool Edit(const char * label, bool & v) { return ImGui::Checkbox(label, &v); }
inline bool Edit(const char * label, float & v) { return ImGui::InputFloat(label, &v); }
inline bool Edit(const char * label, int & v) { return ImGui::InputInt(label, &v); }
inline bool Edit(const char * label, int2 & v) { return ImGui::InputInt2(label, &v.x); }
inline bool Edit(const char * label, int3 & v) { return ImGui::InputInt3(label, &v.x); }
inline bool Edit(const char * label, int4 & v) { return ImGui::InputInt4(label, &v.x); }
inline bool Edit(const char * label, float2 & v) { return ImGui::InputFloat2(label, &v.x); }
inline bool Edit(const char * label, float3 & v) { return ImGui::InputFloat3(label, &v.x); }
inline bool Edit(const char * label, float4 & v) { return ImGui::InputFloat4(label, &v.x); }

// Slider for range_metadata<int>
template<class... A> bool Edit(const char * label, int & f, const A & ... metadata)
{
    auto * rangeData = query_metadata<range_metadata<int>>(metadata...);
    if (rangeData) return ImGui::SliderInt(label, &f, rangeData->min, rangeData->max);
    else return ImGui::InputInt(label, &f);
}

// Slider for range_metadata<float>
template<class... A> bool Edit(const char * label, float & f, const A & ... metadata)
{
    auto * rangeData = query_metadata<range_metadata<float>>(metadata...);
    if (rangeData) return ImGui::SliderFloat(label, &f, rangeData->min, rangeData->max, "%.5f");
    else return ImGui::InputFloat(label, &f);
}

// Slider for range_metadata<float2>
template<class... A> bool Edit(const char * label, float2 & f, const A & ... metadata)
{
    auto * floatRange = query_metadata<range_metadata<float>>(metadata...);
    if (floatRange) return ImGui::SliderFloat2(label, &f[0], floatRange->min, floatRange->max, "%.5f");
    else return ImGui::SliderFloat2(label, &f[0], 0.0f, 1.0f);
}

template<class T, class ... A> bool Edit(const char * label, AssetHandle<T> & h, const A & ... metadata)
{
    auto * hidden = query_metadata<editor_hidden>(metadata...);
    if (hidden) return false;

    int index;
    std::vector<const char *> items;

    for (auto & handle : AssetHandle<T>::list())
    {
        if (handle.name == h.name) index = static_cast<int>(items.size());
        items.push_back(handle.name.c_str());
    }

    if (ImGui::Combo(label, &index, items.data(), static_cast<int>(items.size())))
    {
        h = items[index];
        return true;
    }
    else return false;
}

template<class T> std::enable_if_t<std::is_class<T>::value, bool> Edit(const char * label, T & object)
{
    bool r = false;
    visit_fields(object, [&r](const char * name, auto & field, auto... metadata)
    {
        r |= Edit(name, field, metadata...);
    });
    return r;
}

template<class T> bool InspectGameObjectPolymorphic(const char * label, T * ptr)
{
    bool r = false;
    visit_subclasses(ptr, [&r, label](const char * name, auto * p)
    {
        if (p)
        {
            if (label) r = Edit((std::string(label) + " - " + name).c_str(), *p);
            else r = Edit(name, *p);
        }
    });
    return r;
}

#endif // end imgui_utils_hpp
