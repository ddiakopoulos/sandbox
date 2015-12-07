#pragma once

#ifndef shader_monitor_h
#define shader_monitor_h

#include "GlShared.hpp"
#include "util.hpp"
#include "string_utils.hpp"

#include "third_party/efsw.hpp"

class ShaderMonitor
{
    std::unique_ptr<efsw::FileWatcher> fileWatcher;
    
    struct UpdateListener : public efsw::FileWatchListener
    {
        std::function<void(const std::string filename)> callback;
        void handleFileAction(efsw::WatchID watchid, const std::string & dir, const std::string & filename, efsw::Action action, std::string oldFilename = "")
        {
            if (action == efsw::Actions::Modified)
            {
                if (callback) callback(filename);
            }
        }
    };
    
    std::shared_ptr<GlShader> & program;
    std::string vertexFilename;
    std::string fragmentFilename;
    std::string vPath;
    std::string fPath;
    
    UpdateListener listener;
    std::atomic<bool> shouldRecompile;
    
public:
    
    ShaderMonitor(std::shared_ptr<GlShader> & program, const std::string & vertexShader, const std::string & fragmentShader) : program(program), vPath(vertexShader), fPath(fragmentShader)
    {
        fileWatcher.reset(new efsw::FileWatcher());
        
        efsw::WatchID id = fileWatcher->addWatch("assets/", &listener, true);
        
        vertexFilename = get_filename_with_extension(vertexShader);
        fragmentFilename = get_filename_with_extension(fragmentShader);
        
        listener.callback = [&](const std::string filename)
        {
            std::cout << filename << std::endl;
            if (filename == vertexFilename || filename == fragmentFilename)
            {
                shouldRecompile = true;
            }
        };
        fileWatcher->watch();
    }
    
    // Call this regularly on the gl thread
    void handle_recompile()
    {
        if (shouldRecompile)
        {
            try
            {
                program = std::make_shared<GlShader>(read_file_text(vPath), read_file_text(fPath));
                shouldRecompile = false;
            }
            catch (const std::exception & e)
            {
                std::cout << e.what() << std::endl;
            }
        }
    }
    
};

#endif