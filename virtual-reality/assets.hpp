#pragma once

#ifndef vr_assets_hpp
#define vr_assets_hpp

#include "util.hpp"
#include "linalg_util.hpp"
#include <memory>

template<typename T>
struct UniqueAsset : public Noncopyable
{
    std::string name;
    T asset;
    UniqueAsset(const std::string name, T & asset) : name(name), asset(std::move(asset)) {}
};

template<typename T>
class AssetDatabase
{
    std::map<std::string, std::shared_ptr<UniqueAsset<T>>> table;
public:
    void register_asset(const std::string & name, T && asset)
    {
        table[name] = std::make_shared<UniqueAsset<T>>(name, std::move(asset));
    }
    T & get_asset(const std::string & name)
    {
        return table[name]->asset;
    }
};

#endif // end vr_assets_hpp