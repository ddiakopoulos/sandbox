#include "model-io.hpp"

#include <assert.h>
#include <fstream>
#include "third-party/tinyobj/tiny_obj_loader.h"
#include "third-party/tinyply/tinyply.h"
#include "third-party/meshoptimizer/meshoptimizer.hpp"
#include "fbx-importer.hpp"

std::vector<runtime_skinned_mesh> import_fbx(const std::string & path)
{

}

std::vector<runtime_mesh> import_obj(const std::string & path)
{

}

void optimize_model(runtime_mesh & input)
{
    constexpr size_t cacheSize = 32;

    PostTransformCacheStatistics inputStats = analyzePostTransform(&input.faces[0].x, input.faces.size() * 3, input.vertices.size(), cacheSize);
    std::cout << "input acmr: " << inputStats.acmr << ", cache hit %: " << inputStats.hit_percent << std::endl;

    std::vector<uint32_t> inputIndices;
    std::vector<uint32_t> reorderedIndices(input.faces.size() * 3);

    for (auto f : input.faces)
    {
        inputIndices.push_back(f.x);
        inputIndices.push_back(f.y);
        inputIndices.push_back(f.z);
    }

    {
        optimizePostTransform(reorderedIndices.data(), inputIndices.data(), inputIndices.size(), input.vertices.size(), cacheSize);
    }

    std::vector<float3> reorderedVertexBuffer(input.vertices.size());

    {
        optimizePreTransform(reorderedVertexBuffer.data(), input.vertices.data(), reorderedIndices.data(), reorderedIndices.size(), input.vertices.size(), sizeof(float3));
    }

    input.faces.clear();
    for (int i = 0; i < reorderedIndices.size(); i += 3)
    {
        input.faces.push_back(uint3(reorderedIndices[i], reorderedIndices[i + 1], reorderedIndices[i + 2]));
    }

    input.vertices.swap(reorderedVertexBuffer);

    PostTransformCacheStatistics outStats = analyzePostTransform(&input.faces[0].x, input.faces.size() * 3, input.vertices.size(), cacheSize);
    std::cout << "output acmr: " << outStats.acmr << ", cache hit %: " << outStats.hit_percent << std::endl;
}

runtime_mesh import_mesh_binary(const std::string & path)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.good()) throw std::runtime_error("couldn't open");

    file.seekg(0, std::ios::end);
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    runtime_mesh_binary_header h;
    file.read((char*)&h, sizeof(runtime_mesh_binary_header));

    assert(h.headerVersion == runtime_mesh_binary_version);
    if (h.compressionVersion > 0) assert(h.compressionVersion == runtime_mesh_compression_version);

    runtime_mesh mesh;

    mesh.vertices.resize(h.verticesBytes / sizeof(float3));
    mesh.normals.resize(h.normalsBytes / sizeof(float3));
    mesh.colors.resize(h.colorsBytes / sizeof(float3));
    mesh.texcoord0.resize(h.texcoord0Bytes / sizeof(float2));
    mesh.texcoord1.resize(h.texcoord1Bytes / sizeof(float2));
    mesh.tangents.resize(h.tangentsBytes / sizeof(float3));
    mesh.bitangents.resize(h.bitangentsBytes / sizeof(float3));
    mesh.faces.resize(h.facesBytes / sizeof(uint3));
    mesh.material.resize(h.materialsBytes / sizeof(uint32_t));

    file.read((char*)mesh.vertices.data(), h.verticesBytes);
    file.read((char*)mesh.normals.data(), h.normalsBytes);
    file.read((char*)mesh.colors.data(), h.colorsBytes);
    file.read((char*)mesh.texcoord0.data(), h.texcoord0Bytes);
    file.read((char*)mesh.texcoord1.data(), h.texcoord1Bytes);
    file.read((char*)mesh.tangents.data(), h.tangentsBytes);
    file.read((char*)mesh.bitangents.data(), h.bitangentsBytes);
    file.read((char*)mesh.faces.data(), h.facesBytes);
    file.read((char*)mesh.material.data(), h.materialsBytes);

    return mesh;
}

void export_mesh_binary(const std::string & path, runtime_mesh & mesh, bool compressed = false)
{
    auto file = std::ofstream(path, std::ios::out | std::ios::binary);

    runtime_mesh_binary_header header;
    header.headerVersion = runtime_mesh_binary_version;
    header.compressionVersion = (compressed) ? runtime_mesh_compression_version : 0;
    header.verticesBytes = (uint32_t)mesh.vertices.size() * sizeof(float3);
    header.normalsBytes = (uint32_t)mesh.normals.size() * sizeof(float3);
    header.colorsBytes = (uint32_t)mesh.colors.size() * sizeof(float3);
    header.texcoord0Bytes = (uint32_t)mesh.texcoord0.size() * sizeof(float2);
    header.texcoord1Bytes = (uint32_t)mesh.texcoord1.size() * sizeof(float2);
    header.tangentsBytes = (uint32_t)mesh.tangents.size() * sizeof(float3);
    header.bitangentsBytes = (uint32_t)mesh.bitangents.size() * sizeof(float3);
    header.facesBytes = (uint32_t)mesh.faces.size() * sizeof(uint3);
    header.materialsBytes = (uint32_t)mesh.material.size() * sizeof(uint32_t);

    file.write(reinterpret_cast<char*>(&header), sizeof(runtime_mesh_binary_header));
    file.write(reinterpret_cast<char*>(mesh.vertices.data()), header.verticesBytes);
    file.write(reinterpret_cast<char*>(mesh.normals.data()), header.normalsBytes);
    file.write(reinterpret_cast<char*>(mesh.colors.data()), header.colorsBytes);
    file.write(reinterpret_cast<char*>(mesh.texcoord0.data()), header.texcoord0Bytes);
    file.write(reinterpret_cast<char*>(mesh.texcoord1.data()), header.texcoord1Bytes);
    file.write(reinterpret_cast<char*>(mesh.tangents.data()), header.tangentsBytes);
    file.write(reinterpret_cast<char*>(mesh.bitangents.data()), header.bitangentsBytes);
    file.write(reinterpret_cast<char*>(mesh.faces.data()), header.facesBytes);
    file.write(reinterpret_cast<char*>(mesh.material.data()), header.materialsBytes);

    file.close();
}
