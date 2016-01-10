#ifndef uwidget_h
#define uwidget_h

#include "geometric.hpp"
#include "nvg.hpp"

namespace avl
{
    
struct UIComponent;
struct UIRenderEvent
{
    NVGcontext * ctx;
    UIComponent * parent;
    NvgFont * text;
    NvgFont * icon;
};

struct UIStyleSheet
{
    NVGcolor textColor;
    NVGcolor iconColor;
    NVGcolor foregroundColor;
    NVGcolor backgroundColor;
    NVGcolor borderColor;
};

struct UIComponent
{
    bool acceptInput = true;
    float aspectRatio = 1;
    URect placement = {{0,0},{0,0},{1,0},{1,0}};
    Bounds bounds;
    std::vector<std::shared_ptr<UIComponent>> children;
    
    UIStyleSheet style;
    
    UIComponent(UIStyleSheet stylesheet = UIStyleSheet()) : style(stylesheet) {}
    
    void add_child(const URect & placement, std::shared_ptr<UIComponent> child)
    {
        child->placement = placement;
        children.push_back(child);
    }
    
    void layout()
    {
        for (auto & child : children)
        {
            auto size = child->bounds.get_size();
            child->bounds = child->placement.resolve(bounds);
            auto childAspect = child->bounds.width() / child->bounds.height();
            if (childAspect > 0)
            {
                float xpadding = (1 - std::min((child->bounds.height() * childAspect) / child->bounds.width(), 1.0f)) / 2;
                float ypadding = (1 - std::min((child->bounds.width() / childAspect) / child->bounds.height(), 1.0f)) / 2;
                child->bounds = URect{{xpadding, 0}, {ypadding, 0}, {1 - xpadding, 0}, {1 - ypadding, 0}}.resolve(child->bounds);
            }
            if (child->bounds.get_size() != size) child->layout();
        }
    }
    
    virtual void render(const UIRenderEvent & e) {};
    virtual void input(const InputEvent & e) {};
    virtual void on_mouse_down(const float2 cursor) {};
    virtual void on_mouse_up(const float2 cursor) {};
    virtual void on_mouse_drag(const float2 cursor, const float2 delta) {};
};
    
}

#endif // end uwidget_h
