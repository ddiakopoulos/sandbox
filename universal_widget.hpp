#ifndef uwidget_h
#define uwidget_h

#include "geometric.hpp"

namespace util
{
    
    struct UWidget
    {
        
        math::URect placement = {{0,0},{0,0},{1,0},{1,0}};
        math::Bounds bounds;
        std::vector<std::shared_ptr<UWidget>> children;
        float aspectRatio = 1;
        
        void add_child(const math::URect & placement, std::shared_ptr<UWidget> child)
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
                    child->bounds = math::URect{{xpadding, 0}, {ypadding, 0}, {1 - xpadding, 0}, {1 - ypadding, 0}}.resolve(child->bounds);
                }
                if (child->bounds.get_size() != size)
                    child->layout();
                
                //std::cout << child->bounds.x0 << std::endl;
                //std::cout << child->bounds.y0 << std::endl;
                //std::cout << child->bounds.x1 << std::endl;
                //std::cout << child->bounds.y1 << std::endl;
            }
        }
        
    };

} // end namespace util

#endif // end uwidget_h
