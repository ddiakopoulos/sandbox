#pragma once

#ifndef vr_assets_hpp
#define vr_assets_hpp

#include "util.hpp"
#include "linalg_util.hpp"
#include "gl-api.hpp"
#include "geometry.hpp"
#include "material.hpp"

#include <memory>
#include <vector>
#include <map>

template<typename T>
struct UniqueAsset : public Noncopyable
{
    T asset;
    bool assigned = false;
};

template<typename T>
class AssetHandle
{
    static std::map<std::string, std::shared_ptr<UniqueAsset<T>>> table;
    mutable std::shared_ptr<UniqueAsset<T>> handle{ nullptr };
    AssetHandle(const::std::string & id, std::shared_ptr<UniqueAsset<T>> h) : name(id), handle(h) {} // private constructor for the static list() method below

public:

    std::string name;

    AssetHandle() : AssetHandle("") {}
    AssetHandle(const std::string & asset_id) : AssetHandle(asset_id.c_str()) {}
    AssetHandle(const char * asset_id)
    {
        auto & a = table[asset_id];
        if (!a)
        {
            a = std::make_shared<UniqueAsset<T>>(); // this will construct a handle for an asset that hasn't been assigned yet
        }
        handle = a;
        name = asset_id;
    }

    AssetHandle(const AssetHandle & r)
    {
        handle = r.handle;
        name = r.name;
    }

    T & get() const
    { 
        //std::cout << "Handle is: " << handle << std::endl;
        //std::cout << "GET " << typeid(this).name() << " - " << name << " - " << handle->assigned << std::endl;
        //std::cout << "get assigned?: " << typeid(this).name() << " - " << name << " - " << handle << " // " << handle->assigned << std::endl;

        if (name.size() == 0)
        {
            throw std::invalid_argument("asset has no identifier");
        }
        if (handle)
        {
            return handle->asset;
        }
        else
        {
            auto & a = table[name.c_str()];

            if (!a)
            {
                a = std::make_shared<UniqueAsset<T>>(); // this will construct a handle for an asset that hasn't been assigned yet
                handle = a;
                return handle->asset;
            }
            else
            {
                throw std::runtime_error("no assignment has been made to this asset - " + name);
            }
        }
    }

    T & assign(T && asset)
    {
        handle = std::make_shared<UniqueAsset<T>>();
        handle->asset = std::move(asset);
        handle->assigned = true;
        std::cout << "assigning: " << typeid(this).name() << " - " << name << " - " << handle << " // " << handle->assigned << std::endl;
        return handle->asset;
    }

    bool assigned() const
    {
        std::cout << "assigned?: " << typeid(this).name() << " - " << name << " - " << handle << " // " << handle->assigned << std::endl;
        if (handle->assigned)
        {
            return true;
        }
        else return false;
    }

    static std::vector<AssetHandle> list()
    {
        std::vector<AssetHandle> results;
        for (const auto & a : table)
        {
            results.push_back(AssetHandle<T>(a.first, a.second));
        }
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
typedef AssetHandle<avl::Geometry> GeometryHandle;

#endif // end vr_assets_hpp
