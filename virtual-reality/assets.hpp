#pragma once

#ifndef vr_assets_hpp
#define vr_assets_hpp

#include "util.hpp"
#include "linalg_util.hpp"
#include <memory>
#include <vector>

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
    std::shared_ptr<UniqueAsset> & get_asset(const std::string & name) { return table[name]; }
    std::vector<std::shared_ptr<UniqueAsset<T>>> list()
    {
        std::vector<std::shared_ptr<UniqueAsset<T>>> l;
        for (auto & item : table) l.push_back(item.second);
        return l;
    }
};

#endif // end vr_assets_hpp
