#pragma once

#ifndef shader_monitor_h
#define shader_monitor_h

#include "gl-api.hpp"
#include "util.hpp"
#include "string_utils.hpp"
#include "asset_io.hpp"
#include "third_party/efsw/efsw.hpp"
#include <regex>

namespace avl
{

    inline std::string preprocess_includes(const std::string & source, const std::string & includeSearchPath, std::vector<std::string> & includes, int depth)
    {
        if (depth > 4) throw std::runtime_error("exceeded max include recursion depth");

        static const std::regex re("^[ ]*#[ ]*include[ ]+[\"<](.*)[\">].*");
        std::stringstream input;
        std::stringstream output;

        input << source;

        size_t lineNumber = 1;
        std::smatch matches;
        std::string line;

        while (std::getline(input, line))
        {
            if (std::regex_search(line, matches, re))
            {
                std::string includeFile = matches[1];
                std::string includeString = read_file_text(includeSearchPath + "/" + includeFile);

                if (!includeFile.empty())
                {
                    includes.push_back(includeSearchPath + "/" + includeFile);
                    output << preprocess_includes(includeString, includeSearchPath, includes, depth++) << std::endl;
                }
            }
            else
            {
                output << "#line " << lineNumber << std::endl;
                output << line << std::endl;
            }
            ++lineNumber;
        }
        return output.str();
    }

    inline std::string preprocess_version(const std::string & source)
    {
        std::stringstream input;
        std::stringstream output;

        input << source;

        size_t lineNumber = 1;
        std::string line;
        std::string version;

        while (std::getline(input, line))
        {
            if (line.find("#version") != std::string::npos) version = line;
            else output << line << std::endl;
            ++lineNumber;
        }

        std::stringstream result;
        result << version << std::endl << output.str();
        return result.str();
    }

    inline GlShader preprocess(
        const std::string & vertexShader, 
        const std::string & fragmentShader, 
        const std::string & geomShader,
        const std::string & includeSearchPath, 
        const std::vector<std::string> & defines, 
        std::vector<std::string> & includes)
    {
        std::stringstream vertex;
        std::stringstream fragment;
        std::stringstream geom;

        for (const auto define : defines)
        {
            if (vertexShader.size()) vertex << "#define " << define << std::endl;
            if (fragmentShader.size()) fragment << "#define " << define << std::endl;
            if (geomShader.size()) geom << "#define " << define << std::endl;
        }

        if (vertexShader.size()) vertex << vertexShader;
        if (fragmentShader.size()) fragment << fragmentShader;
        if (geomShader.size()) geom << geomShader;

        if (geomShader.size())
        {
            return GlShader(
                preprocess_version(preprocess_includes(vertex.str(), includeSearchPath, includes, 0)),
                preprocess_version(preprocess_includes(fragment.str(), includeSearchPath, includes, 0)),
                preprocess_version(preprocess_includes(geom.str(), includeSearchPath, includes, 0)));
        }
        else
        {
            return GlShader(
                preprocess_version(preprocess_includes(vertex.str(), includeSearchPath, includes, 0)),
                preprocess_version(preprocess_includes(fragment.str(), includeSearchPath, includes, 0)));
        }
    }

    class ShaderMonitor
    {
        std::unique_ptr<efsw::FileWatcher> fileWatcher;

        struct ShaderAsset
        {
            std::function<void(GlShader)> onModified;

            std::string vertexPath;
            std::string fragmentPath;
            std::string geomPath;
            std::string includePath;
            std::vector<std::string> defines;
            std::vector<std::string> includes;

            bool shouldRecompile = false;

            ShaderAsset(
                const std::string & v, 
                const std::string & f, 
                const std::string & g = "", 
                const std::string & inc = "", 
                const std::vector<std::string> & def = {}) : vertexPath(v), fragmentPath(f), geomPath(g), includePath(inc), defines(def) { };

            void recompile()
            {
                shouldRecompile = false;

                GlShader result;

                try
                {
                    if (defines.size() > 0 || includePath.size() > 0)
                    {
                        result = preprocess(read_file_text(vertexPath), read_file_text(fragmentPath), read_file_text(geomPath), includePath, defines, includes);
                    }
                    else
                    {
                        result = GlShader(read_file_text(vertexPath), read_file_text(fragmentPath), read_file_text(geomPath));
                    }
                }
                catch (const std::exception & e)
                {
                    std::cout << "Shader recompilation error: " << e.what() << std::endl;
                }

                if (onModified) onModified(std::move(result));
            }
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

        std::vector<ShaderAsset> assets;

    public:

        ShaderMonitor(const std::string & basePath)
        {
            fileWatcher.reset(new efsw::FileWatcher());

            efsw::WatchID id = fileWatcher->addWatch(basePath, &listener, true);

            listener.callback = [&](const std::string filename)
            {
                for (auto & shader : assets)
                {
                    // Recompile if any one of the shader stages have changed
                    if (get_filename_with_extension(filename) == get_filename_with_extension(shader.vertexPath) || 
                        get_filename_with_extension(filename) == get_filename_with_extension(shader.fragmentPath) || 
                        get_filename_with_extension(filename) == get_filename_with_extension(shader.geomPath))
                    {
                        shader.shouldRecompile = true;
                    }

                    // Each shader keeps a list of the files it includes. ShaderMonitor watches a base path,
                    // so we should be able to recompile shaders depenent on common includes
                    for (auto & includePath : shader.includes)
                    {
                        if (get_filename_with_extension(filename) == get_filename_with_extension(includePath))
                        {
                            shader.shouldRecompile = true;
                            break;
                        }
                    }
                }
            };

            fileWatcher->watch();
        }

        // Call this regularly on the gl thread
        void handle_recompile()
        {
            for (auto & shader : assets)
            {
                if (shader.shouldRecompile) shader.recompile();
            }
        }

        // Watch vertex, fragment, and geometry
        void watch(
            const std::string & vertexShader,
            const std::string & fragmentShader,
            std::function<void(GlShader)> callback)
        {
            ShaderAsset asset(vertexShader, fragmentShader);
            asset.onModified = callback;
            asset.recompile();
            assets.push_back(std::move(asset));
        }

        // Watch vertex, fragment, and geometry
        void watch(
            const std::string & vertexShader,
            const std::string & fragmentShader,
            const std::string & geometryShader, 
            std::function<void(GlShader)> callback)
        {
            ShaderAsset asset(vertexShader, fragmentShader, geometryShader);
            asset.onModified = callback;
            asset.recompile();
            assets.push_back(std::move(asset));
        }

        // Watch vertex and fragment with includes and defines
        void watch(
            const std::string & vertexShader, 
            const std::string & fragmentShader,
            const std::string & includePath,
            const std::vector<std::string> & defines,
            std::function<void(GlShader)> callback)
        {
            ShaderAsset asset(vertexShader, fragmentShader, "", includePath, defines);
            asset.onModified = callback;
            asset.recompile();
            assets.push_back(std::move(asset));
        }

        // Watch vertex and fragment and geometry with includes and defines
        void watch(
            const std::string & vertexShader,
            const std::string & fragmentShader,
            const std::string & geometryShader,
            const std::string & includePath,
            const std::vector<std::string> & defines,
            std::function<void(GlShader)> callback)
        {
            ShaderAsset asset(vertexShader, fragmentShader, geometryShader, includePath, defines);
            asset.onModified = callback;
            asset.recompile();
            assets.push_back(std::move(asset));
        }

    };

}

#endif
