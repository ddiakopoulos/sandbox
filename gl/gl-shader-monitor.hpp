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

    inline std::unique_ptr<GlShader> preprocess(
        const std::string & vertexShader, 
        const std::string & fragmentShader, 
        const std::string & includeSearchPath, 
        const std::vector<std::string> & defines, 
        std::vector<std::string> & includes)
    {
        std::stringstream vertex;
        std::stringstream fragment;

        for (const auto define : defines)
        {
            if (vertexShader.size()) vertex << "#define " << define << std::endl;
            if (fragmentShader.size()) fragment << "#define " << define << std::endl;
        }

        if (vertexShader.size()) vertex << vertexShader;
        if (fragmentShader.size()) fragment << fragmentShader;

        std::unique_ptr<GlShader> result;

        result.reset(new GlShader(
            preprocess_version(preprocess_includes(vertex.str(), includeSearchPath, includes, 0)),
            preprocess_version(preprocess_includes(fragment.str(), includeSearchPath, includes, 0))));

        return result;
    }

    class ShaderMonitor
    {
        std::unique_ptr<efsw::FileWatcher> fileWatcher;

        struct ShaderAsset
        {
            std::shared_ptr<GlShader> program;
            std::string vertexPath;
            std::string fragmentPath;
            std::string geomPath;
            std::string includePath;
            std::vector<std::string> defines;
            std::vector<std::string> includes;
            bool shouldRecompile = false;
            std::string errorStr;
            ShaderAsset(
                std::shared_ptr<GlShader> program, 
                const std::string & v, 
                const std::string & f, 
                const std::string & g = "", 
                const std::string & inc = "", 
                const std::vector<std::string> & defines = {},
                const std::vector<std::string> & includePaths = {})
                : program(program), vertexPath(v), fragmentPath(f), geomPath(g), includePath(inc), defines(defines), includes(includePaths) { };
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
                    // Recompile if any one of the shader stages have changed
                    if (get_filename_with_extension(filename) == get_filename_with_extension(shader->vertexPath) || 
                        get_filename_with_extension(filename) == get_filename_with_extension(shader->fragmentPath) || 
                        get_filename_with_extension(filename) == get_filename_with_extension(shader->geomPath))
                    {
                        shader->shouldRecompile = true;
                    }

                    // Each shader keeps a list of the files it includes. ShaderMonitor watches a base path,
                    // so we should be able to recompile shaders depenent on common includes
                    for (auto & includePath : shader->includes)
                    {
                        if (get_filename_with_extension(filename) == get_filename_with_extension(includePath))
                        {
                            shader->shouldRecompile = true;
                            break;
                        }
                    }
                }
            };

            fileWatcher->watch();
        }

        // Watch vertex and fragment
        std::shared_ptr<GlShader> watch(
            const std::string & vertexShader, 
            const std::string & fragmentShader)
        {
            std::shared_ptr<GlShader> watchedShader = std::make_shared<GlShader>(read_file_text(vertexShader), read_file_text(fragmentShader));
            shaders.emplace_back(new ShaderAsset(watchedShader, vertexShader, fragmentShader, "", ""));
            return watchedShader;
        }

        // Watch vertex, fragment, and geometry
        std::shared_ptr<GlShader> watch(
            const std::string & vertexShader, 
            const std::string & fragmentShader, 
            const std::string & geometryShader)
        {
            std::shared_ptr<GlShader> watchedShader = std::make_shared<GlShader>(read_file_text(vertexShader), read_file_text(fragmentShader), read_file_text(geometryShader));
            shaders.emplace_back(new ShaderAsset(watchedShader, vertexShader, fragmentShader, geometryShader, ""));
            return watchedShader;
        }

        // Watch vertex and fragment with includes and defines
        std::shared_ptr<GlShader> watch(
            const std::string & vertexShader, 
            const std::string & fragmentShader,
            const std::string & includePath,
            const std::vector<std::string> & defines)
        {
            // Preprocess this bundle
            std::vector<std::string> includes;
            auto src = preprocess(read_file_text(vertexShader), read_file_text(fragmentShader), includePath, defines, includes);
            std::shared_ptr<GlShader> watchedShader(std::move(src));
            
            // Add to watched assets
            ShaderAsset * asset = new ShaderAsset(watchedShader, vertexShader, fragmentShader, "", includePath, defines, includes);
            shaders.emplace_back(asset);

            // Return shared_ptr to GlShader
            return watchedShader;
        }

        // Watch vertex, fragment, and geometry with includes and defines
        std::shared_ptr<GlShader> watch(
            const std::string & vertexShader, 
            const std::string & fragmentShader, 
            const std::string & geometryShader,
            const std::string & includePath,
            const std::vector<std::string> & defines)
        {
            std::shared_ptr<GlShader> watchedShader = std::make_shared<GlShader>(read_file_text(vertexShader), read_file_text(fragmentShader), read_file_text(geometryShader));
            shaders.emplace_back(new ShaderAsset(watchedShader, vertexShader, fragmentShader, geometryShader, includePath, defines));
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

                        // Needs to be run through the preprocessor
                        if (shader->defines.size() > 0 || shader->includePath.size() > 0)
                        {
                            std::vector<std::string> includes;
                            auto src = preprocess(read_file_text(shader->vertexPath), read_file_text(shader->fragmentPath), shader->includePath, shader->defines, includes);
                            shader->program = std::move(src);
                        }
                        else
                        {
                            shader->program = std::make_shared<GlShader>(read_file_text(shader->vertexPath), read_file_text(shader->fragmentPath), read_file_text(shader->geomPath));
                        }

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
