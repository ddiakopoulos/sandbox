#pragma once

#ifndef vr_assets_hpp
#define vr_assets_hpp

#include "util.hpp"
#include "linalg_util.hpp"
#include "GL_API.hpp"
#include <memory>
#include <vector>
#include <map>

template<typename T>
struct UniqueAsset : public Noncopyable
{
    std::string name;
    T asset;
    UniqueAsset(const std::string name, T && asset) : name(name), asset(std::move(asset)) {}
};

template<typename T>
class AssetDatabase : public Noncopyable
{
    std::map<std::string, std::shared_ptr<UniqueAsset<T>>> table;
public:
    void register_asset(const std::string & name, T && asset) { table[name] = std::make_shared<UniqueAsset<T>>(name, std::move(asset)); }
    std::shared_ptr<UniqueAsset<T>> get_asset(const std::string & name) 
    { 
        auto r = table[name]; 
        if (r) return r; 
        else throw std::invalid_argument("no asset for this name");
    }
    std::shared_ptr<UniqueAsset<T>> operator[](const std::string & name) { return get_asset(name); }
    std::vector<std::shared_ptr<UniqueAsset<T>>> list()
    {
        std::vector<std::shared_ptr<UniqueAsset<T>>> l;
        for (auto & item : table) l.push_back(item.second);
        return l;
    }
};

typedef std::shared_ptr<UniqueAsset<GlTexture2D>> texture_handle;

#endif // end vr_assets_hpp