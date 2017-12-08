#pragma once

#ifndef vr_assets_hpp
#define vr_assets_hpp

#include "util.hpp"
#include "math-core.hpp"
#include "gl-api.hpp"
#include "geometry.hpp"

#include <memory>
#include <unordered_map>

template<typename T>
struct UniqueAsset : public Noncopyable
{
    T asset;
    bool assigned = false;
    std::string filepath;
    std::string timestamp;
};

template<typename T>
class AssetHandle
{
    static std::unordered_map<std::string, std::shared_ptr<UniqueAsset<T>>> table;
    mutable std::shared_ptr<UniqueAsset<T>> handle{ nullptr };
    AssetHandle(const::std::string & id, std::shared_ptr<UniqueAsset<T>> h) : name(id), handle(h) {} // private constructor for the static list() method below

public:

    std::string name;

    AssetHandle() : AssetHandle("") {}
    AssetHandle(const std::string & asset_id) : AssetHandle(asset_id.c_str()) {}
    AssetHandle(const char * asset_id)
    {
        auto & a = table[asset_id];
        if (!a) a = std::make_shared<UniqueAsset<T>>();
        handle = a;
        name = asset_id;
    }

    AssetHandle(const AssetHandle & r)
    {
        handle = r.handle;
        name = r.name;
    }

    // Return const reference to underlying resource. Note that `UniqueAsset` is default constructable,
    // so this function may not return the desired asset if loading has failed.
    T & get() const
    { 
        // if (name.size() == 0) throw std::invalid_argument("asset has no identifier");
        
        if (handle->assigned) return handle->asset; // Check if this handle has a cached asset
        else // If not, this is a virgin handle and we should lookup from the static table
        {
            auto & a = table[name];
            if (a)
            {
                handle = a;
                return handle->asset;
            }
        }

        // No valid asset was found...
        std::cout << "Returning default-constructed asset for " << name << std::endl;
        return handle->asset;
    }

    T & assign(T && asset)
    {
        std::cout << "Assigning: " << typeid(this).name() << " - " << name << " - " << handle << " // " << handle->assigned << std::endl;
        handle->asset = std::move(asset);
        handle->assigned = true;
        return handle->asset;
    }

    // Since `AssetHandle`s can serve back default-constructed values (handle->assigned == false)
    // this also needs to lookup back to the table in the case that we check if an asset has 
    // been assigned before ever calling `get()`
    bool assigned() const
    {
        auto & a = table[name];
        if (a) handle = a;
        if (handle->assigned) return true;
        return false;
    }

    static std::vector<AssetHandle> list()
    {
        std::vector<AssetHandle> results;
        for (const auto & a : table) results.push_back(AssetHandle<T>(a.first, a.second));
        return results;
    }
};

template<class T> 
std::unordered_map<std::string, std::shared_ptr<UniqueAsset<T>>> AssetHandle<T>::table;

template<class T> AssetHandle<T> create_handle_for_asset(const char * asset_id, T && asset)
{
    AssetHandle<T> assetHandle(asset_id);
    assetHandle.assign(std::move(asset));
    return assetHandle;
}

typedef AssetHandle<GlTexture2D> GlTextureHandle;
typedef AssetHandle<GlShader> GlShaderHandle;
typedef AssetHandle<GlMesh> GlMeshHandle;
typedef AssetHandle<avl::Geometry> GeometryHandle;

#endif // end vr_assets_hpp
