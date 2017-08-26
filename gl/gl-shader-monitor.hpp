#pragma once

#ifndef shader_monitor_h
#define shader_monitor_h

#include "gl-api.hpp"
#include "util.hpp"
#include "string_utils.hpp"
#include "third_party/efsw/efsw.hpp"

namespace avl
{

class ShaderMonitor
{
    std::unique_ptr<efsw::FileWatcher> fileWatcher;
    
    struct ShaderAsset
    {
        GlShader & program;
        std::string vertexPath;
        std::string fragmentPath;
        std::string geomPath;
        bool shouldRecompile = false;
        std::string errorStr;
        ShaderAsset(GlShader & program, const std::string & v, const std::string & f, const std::string & g = "") : program(program), vertexPath(v), fragmentPath(f), geomPath(g) {};
    };
    
    struct UpdateListener : public efsw::FileWatchListener
    {
        std::function<void(const std::string filename)> callback;
        void handleFileAction(efsw::WatchID watchid, const std::string & dir, const std::string & filename, efsw::Action action, std::string oldFilename = "")
        {
            if (action == efsw::Actions::Modified)
            {
                std::cout << "Shader file updated: " << filename << std::endl;
                if (callback) callback(filename);
            }
        }
    };

    UpdateListener listener;
    
    std::vector<std::unique_ptr<ShaderAsset>> shaders;
    
public:
    
    ShaderMonitor(const std::string & basePath)
    {
        fileWatcher.reset(new efsw::FileWatcher());
        
        efsw::WatchID id = fileWatcher->addWatch(basePath, &listener, true);
        
        listener.callback = [&](const std::string filename)
        {
            for (auto & shader : shaders)
            {
                if (get_filename_with_extension(filename) == get_filename_with_extension(shader->vertexPath) || get_filename_with_extension(filename) == get_filename_with_extension(shader->fragmentPath))
                {
                    shader->shouldRecompile = true;
                }
            }
        };

        fileWatcher->watch();
    }

    std::shared_ptr<GlShader> watch(const std::string & vertexShader, const std::string & fragmentShader, const std::string geomPath = "")
    {
        std::shared_ptr<GlShader> watchedShader = std::make_shared<GlShader>(read_file_text(vertexShader), read_file_text(fragmentShader), read_file_text(geomPath));
        shaders.emplace_back(new ShaderAsset(*watchedShader.get(), vertexShader, fragmentShader, geomPath));
        return watchedShader;
    }
    
    // Call this regularly on the gl thread
    void handle_recompile()
    {
        for (auto & shader : shaders)
        {
            if (shader->shouldRecompile)
            {
                try
                {
                    shader->shouldRecompile = false;
                    shader->program = GlShader(read_file_text(shader->vertexPath), read_file_text(shader->fragmentPath), read_file_text(shader->geomPath));
                    if (shader->errorStr.length() > 0)
                    {
                        shader->errorStr = "";
                        std::cout << "Shader recompiled successfully" << std::endl;
                    }
                }
                catch (const std::exception & e)
                {
                    shader->errorStr = e.what();
                    std::cout << shader->errorStr << std::endl;
                }
            }
        }
    }
    
};
    
}

#endif
