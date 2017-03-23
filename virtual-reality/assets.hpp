#pragma once

#ifndef vr_assets_hpp
#define vr_assets_hpp

#include "GL_API.hpp"
#include "linalg_util.hpp"
#include "geometric.hpp"

template<typename T>
class AssetDatabase
{
    std::map<std::string, T> table;
public:
    void register_asset(const std::string & name, T && asset)
    {
        table[name] = std::move(asset);
    }
    T & get_asset(const std::string & name)
    {
        return table[name];
    }
};

#endif // end vr_assets_hpp