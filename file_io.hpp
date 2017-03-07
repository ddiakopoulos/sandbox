#ifndef file_io_h
#define file_io_h

#include <exception>
#include <vector>
#include <string>
#include <fstream>
#include <streambuf>
#include "third_party/stb/stb_image.h"
#include "GL_API.hpp"

namespace avl
{
    
    inline std::vector<uint8_t> read_file_binary(const std::string pathToFile)
    {
        FILE * f = fopen(pathToFile.c_str(), "rb");
        
        if (!f) throw std::runtime_error("file not found");
        
        fseek(f, 0, SEEK_END);
        size_t lengthInBytes = ftell(f);
        fseek(f, 0, SEEK_SET);
        
        std::vector<uint8_t> fileBuffer(lengthInBytes);
        
        size_t elementsRead = fread(fileBuffer.data(), 1, lengthInBytes, f);
        
        if (elementsRead == 0 || fileBuffer.size() < 4) throw std::runtime_error("error reading file or file too small");
        
        fclose(f);
        return fileBuffer;
    }
    
    inline std::string read_file_text(const std::string & pathToFile)
    {
        std::ifstream t(pathToFile);
        std::string str ((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());
        return str;
    }

    inline std::vector<uint8_t> load_image_data(const std::string & path)
    {
        auto binaryFile = read_file_binary(path);

        int width, height, nBytes;
        auto data = stbi_load_from_memory(binaryFile.data(), (int)binaryFile.size(), &width, &height, &nBytes, 0);
        std::vector<uint8_t> d(width * height * nBytes);
        memcpy(d.data(), data, nBytes * width * height);
        stbi_image_free(data);
        return d;
    }

    #include "third_party/stb/stb_image.h" 

    inline GlTexture2D load_image(const std::string & path)
    {
        auto binaryFile = avl::read_file_binary(path);

        int width, height, nBytes;
        auto data = stbi_load_from_memory(binaryFile.data(), (int)binaryFile.size(), &width, &height, &nBytes, 0);

        GlTexture2D tex;
        switch (nBytes)
        {
        case 3: tex.setup(width, height, GL_RGB, GL_RGB, GL_UNSIGNED_BYTE, data, true); break;
        case 4: tex.setup(width, height, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, data, true); break;
        default: throw std::runtime_error("unsupported number of channels");
        }
        tex.set_name(path);
        stbi_image_free(data);
        return tex;
    }

}

#endif // file_io_h
