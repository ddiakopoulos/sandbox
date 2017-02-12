#pragma once

#ifndef image_hpp
#define image_hpp

#include "GL_API.hpp"
#include "file_io.hpp"
#include "third_party/stb/stb_image.h" 

inline avl::GlTexture2D load_image(const std::string & path)
{
	auto binaryFile = avl::read_file_binary(path);

	int width, height, nBytes;
	auto data = stbi_load_from_memory(binaryFile.data(), (int)binaryFile.size(), &width, &height, &nBytes, 0);

	avl::GlTexture2D tex;
	switch (nBytes)
	{
		case 3: tex.setup(width, height, GL_RGB, GL_RGB, GL_UNSIGNED_BYTE, data, true); break;
		case 4: tex.setup(width, height, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, data, true); break;
		default: throw std::runtime_error("supported number of channels");
	}
	tex.set_name(path);
	stbi_image_free(data);
	return tex;
}

#endif