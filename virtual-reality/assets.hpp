#pragma once

#ifndef vr_assets_hpp
#define vr_assets_hpp

#include "util.hpp"
#include "linalg_util.hpp"
#include "gl-api.hpp"
#include "geometry.hpp"

#include <memory>
#include <vector>
#include <map>

template<typename T>
struct UniqueAsset : public Noncopyable
{
    T asset;
    bool assigned = false;
    const std::string name;
    UniqueAsset(const std::string name) : name(name) {}
};

template<typename T>
class AssetHandle
{
    static std::map<std::string, std::shared_ptr<UniqueAsset<T>>> table;
    std::shared_ptr<UniqueAsset<T>> handle;
    AssetHandle(std::shared_ptr<UniqueAsset<T>> h) : handle(h) {} // private constructor for the static list() method below

public:

    AssetHandle() : AssetHandle("") {}
    AssetHandle(const std::string & asset_id) : AssetHandle(asset_id.c_str()) {}
    AssetHandle(const char * asset_id)
    {
        auto & a = table[asset_id];
        if (!a) a = std::make_shared<UniqueAsset<T>>(asset_id);
        handle = a;
    }

    T & get() const
    { 
        if (handle->assigned) return handle->asset;
        else throw std::runtime_error("no assignment has been made to this asset - " + handle->name);
    }

    T & assign(T && asset)
    {
        handle->asset = std::move(asset);
        handle->assigned = true;
        return handle->asset;
    }

    bool assigned() const
    {
        if (handle->assigned) return true;
        else return false;
    }

    std::string asset_id() const
    {
        if (handle->assigned) return handle->name;
        else return std::string("");
    }

    static std::vector<AssetHandle> list()
    {
        std::vector<AssetHandle> results;
        for (const auto & a : table) results.push_back(a.second);
        return results;
    }
};

template<class T> 
std::map<std::string, std::shared_ptr<UniqueAsset<T>>> AssetHandle<T>::table;

template<class T> void global_register_asset(const char * asset_id, T && asset)
{
    AssetHandle<T> assetHandle(asset_id);
    assetHandle.assign(std::move(asset));
}

typedef AssetHandle<GlTexture2D> GlTextureHandle;
typedef AssetHandle<GlShader> GlShaderHandle;
typedef AssetHandle<GlMesh> GlMeshHandle;
typedef AssetHandle<Geometry> GeometryHandle;

#endif // end vr_assets_hpp
