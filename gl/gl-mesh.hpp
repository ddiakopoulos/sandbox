#ifndef gl_mesh_hpp
#define gl_mesh_hpp

#include "math-core.hpp"
#include "string_utils.hpp"
#include "gl-api.hpp"
#include "file_io.hpp"
#include "asset_io.hpp"
#include "tinyply.h"
#include "tiny_obj_loader.h"
#include "geometry.hpp"

#include <sstream>
#include <vector>
#include <fstream>
#include <algorithm>
#include <assert.h>

#if defined(ANVIL_PLATFORM_WINDOWS)
#pragma warning(push)
#pragma warning(disable : 4244)
#pragma warning(disable : 4267)
#endif

namespace avl
{
    struct TexturedMeshChunk
    {
        std::vector<int> materialIds;
        GlMesh mesh;
    };

    struct TexturedMesh
    {
        std::vector<TexturedMeshChunk> chunks;
        std::vector<GlTexture2D> textures;
    };

    inline GlMesh make_mesh_from_geometry(const Geometry & geometry, const GLenum usage = GL_STATIC_DRAW)
    {
        assert(geometry.vertices.size() > 0);

        GlMesh m;

        int vertexOffset = 0;
        int normalOffset = 0;
        int colorOffset = 0;
        int texOffset = 0;
        int tanOffset = 0;
        int bitanOffset = 0;

        int components = 3;

        if (geometry.normals.size() != 0)
        {
            normalOffset = components; components += 3;
        }

        if (geometry.colors.size() != 0)
        {
            colorOffset = components; components += 3;
        }

        if (geometry.texCoords.size() != 0)
        {
            texOffset = components; components += 2;
        }

        if (geometry.tangents.size() != 0)
        {
            tanOffset = components; components += 3;
        }

        if (geometry.bitangents.size() != 0)
        {
            bitanOffset = components; components += 3;
        }

        std::vector<float> buffer;
        buffer.reserve(geometry.vertices.size() * components);

        for (size_t i = 0; i < geometry.vertices.size(); ++i)
        {
            buffer.push_back(geometry.vertices[i].x);
            buffer.push_back(geometry.vertices[i].y);
            buffer.push_back(geometry.vertices[i].z);

            if (normalOffset)
            {
                buffer.push_back(geometry.normals[i].x);
                buffer.push_back(geometry.normals[i].y);
                buffer.push_back(geometry.normals[i].z);
            }

            if (colorOffset)
            {
                buffer.push_back(geometry.colors[i].x);
                buffer.push_back(geometry.colors[i].y);
                buffer.push_back(geometry.colors[i].z);
            }

            if (texOffset)
            {
                buffer.push_back(geometry.texCoords[i].x);
                buffer.push_back(geometry.texCoords[i].y);
            }

            if (tanOffset)
            {
                buffer.push_back(geometry.tangents[i].x);
                buffer.push_back(geometry.tangents[i].y);
                buffer.push_back(geometry.tangents[i].z);
            }

            if (bitanOffset)
            {
                buffer.push_back(geometry.bitangents[i].x);
                buffer.push_back(geometry.bitangents[i].y);
                buffer.push_back(geometry.bitangents[i].z);
            }
        }

        // Hereby known as the The Blake C. Lucas mesh attribute order:
        m.set_vertex_data(buffer.size() * sizeof(float), buffer.data(), usage);
        m.set_attribute(0, 3, GL_FLOAT, GL_FALSE, components * sizeof(float), ((float*)0) + vertexOffset);
        if (normalOffset) m.set_attribute(1, 3, GL_FLOAT, GL_FALSE, components * sizeof(float), ((float*)0) + normalOffset);
        if (colorOffset) m.set_attribute(2, 3, GL_FLOAT, GL_FALSE, components * sizeof(float), ((float*)0) + colorOffset);
        if (texOffset) m.set_attribute(3, 2, GL_FLOAT, GL_FALSE, components * sizeof(float), ((float*)0) + texOffset);
        if (tanOffset) m.set_attribute(4, 3, GL_FLOAT, GL_FALSE, components * sizeof(float), ((float*)0) + tanOffset);
        if (bitanOffset) m.set_attribute(5, 3, GL_FLOAT, GL_FALSE, components * sizeof(float), ((float*)0) + bitanOffset);

        if (geometry.faces.size() > 0)
        {
            m.set_elements(geometry.faces, usage);
        }

        return m;
    }

    inline size_t make_vert(std::vector<std::tuple<float3, float2>> & buffer, const float3 & position, float2 texcoord)
    {
        auto vert = std::make_tuple(position, texcoord);
        auto it = std::find(begin(buffer), end(buffer), vert);
        if (it != end(buffer)) return it - begin(buffer);
        buffer.push_back(vert); // Add to unique list if we didn't find it
        return (size_t) buffer.size() - 1;
    }

    // Handles trimeshes only
    inline Geometry load_geometry_from_ply(const std::string & path, bool smooth = false)
    {
        Geometry geo;

        try
        {
            std::ifstream ss(path, std::ios::binary);

            if (ss.rdbuf()->in_avail())
                throw std::runtime_error("Could not load file" + path);

            tinyply::PlyFile file(ss);

            std::vector<float> verts;
            std::vector<int32_t> faces;
            std::vector<float> texCoords;

            bool hasTexcoords = false;

            // Todo... usual suspects like normals and vertex colors
            for (auto e : file.get_elements())
                for (auto p : e.properties)
                    if (p.name == "texcoord") hasTexcoords = true;

            uint32_t vertexCount = file.request_properties_from_element("vertex", { "x", "y", "z" }, verts);
            uint32_t numTriangles = file.request_properties_from_element("face", { "vertex_indices" }, faces, 3);
            uint32_t uvCount = (hasTexcoords) ? file.request_properties_from_element("face", { "texcoord" }, texCoords, 6) : 0;

            file.read(ss);

            geo.vertices.reserve(vertexCount);
            std::vector<float3> flatVerts;
            for (uint32_t i = 0; i < vertexCount * 3; i += 3)
                flatVerts.push_back(float3(verts[i], verts[i + 1], verts[i + 2]));

            geo.faces.reserve(numTriangles);
            std::vector<uint3> flatFaces;
            for (uint32_t i = 0; i < numTriangles * 3; i += 3)
                flatFaces.push_back(uint3(faces[i], faces[i + 1], faces[i + 2]));

            geo.texCoords.reserve(uvCount);
            std::vector<float2> flatTexCoords;
            for (uint32_t i = 0; i < uvCount * 6; i += 2)
                flatTexCoords.push_back(float2(texCoords[i], texCoords[i + 1]));

            std::vector<std::tuple<float3, float2>> uniqueVerts;
            std::vector<uint32_t> indexBuffer;

            // Create unique vertices for existing 'duplicate' vertices that have different texture coordinates
            if (flatTexCoords.size())
            {
                for (int i = 0; i < flatFaces.size(); i++)
                {
                    auto f = flatFaces[i];
                    indexBuffer.push_back(make_vert(uniqueVerts, flatVerts[f.x], flatTexCoords[3 * i + 0]));
                    indexBuffer.push_back(make_vert(uniqueVerts, flatVerts[f.y], flatTexCoords[3 * i + 1]));
                    indexBuffer.push_back(make_vert(uniqueVerts, flatVerts[f.z], flatTexCoords[3 * i + 2]));
                }

                for (auto v : uniqueVerts)
                {
                    geo.vertices.push_back(std::get<0>(v));
                    geo.texCoords.push_back(std::get<1>(v));
                }
            }
            else
            {
                geo.vertices.reserve(flatVerts.size());
                for (auto v : flatVerts) geo.vertices.push_back(v);
            }

            if (indexBuffer.size())
            {
                for (int i = 0; i < indexBuffer.size(); i += 3) geo.faces.push_back(uint3(indexBuffer[i], indexBuffer[i + 1], indexBuffer[i + 2]));
            }
            else geo.faces = flatFaces;

            geo.compute_normals(smooth);

            if (faces.size() && flatTexCoords.size()) geo.compute_tangents();

            geo.compute_bounds();
        }
        catch (const std::exception & e)
        {
            ANVIL_ERROR("[tinyply] Caught exception:" << e.what());
        }

        return geo;
    }

    inline std::vector<Geometry> load_geometry_from_obj_no_texture(const std::string & asset)
    {
        std::vector<Geometry> meshList;

        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;

        std::string err;
        bool status = tinyobj::LoadObj(shapes, materials, err, asset.c_str());

        if (status && !err.empty()) std::cerr << "tinyobj sys: " + err << std::endl;

        // Parse tinyobj data into geometry struct
        for (unsigned int i = 0; i < shapes.size(); i++)
        {
            Geometry g;
            tinyobj::mesh_t *mesh = &shapes[i].mesh;

            for (size_t i = 0; i < mesh->indices.size(); i += 3)
            {
                uint32_t idx1 = mesh->indices[i + 0];
                uint32_t idx2 = mesh->indices[i + 1];
                uint32_t idx3 = mesh->indices[i + 2];
                g.faces.push_back({ idx1, idx2, idx3 });
            }

            for (size_t i = 0; i < mesh->texcoords.size(); i += 2)
            {
                float uv1 = mesh->texcoords[i + 0];
                float uv2 = mesh->texcoords[i + 1];
                g.texCoords.push_back({ uv1, uv2 });
            }

            for (size_t v = 0; v < mesh->positions.size(); v += 3)
            {
                float3 vert = float3(mesh->positions[v + 0], mesh->positions[v + 1], mesh->positions[v + 2]);
                g.vertices.push_back(vert);
            }

            if (g.normals.size() == 0) g.compute_normals(false);

            meshList.push_back(g);
        }

        return meshList;
    }

    inline TexturedMesh load_geometry_from_obj(const std::string & asset, bool printDebug = false)
    {
        TexturedMesh mesh;

        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;

        std::string parentDir = parent_directory_from_filepath(asset) + "/";

        std::string err;
        bool status = tinyobj::LoadObj(shapes, materials, err, asset.c_str(), parentDir.c_str());

        if (status && !err.empty())
            throw std::runtime_error("tinyobj exception: " + err);

        std::vector<Geometry> submesh;

        if (printDebug) std::cout << "# of shapes    : " << shapes.size() << std::endl;
        if (printDebug) std::cout << "# of materials : " << materials.size() << std::endl;

        // Parse tinyobj data into geometry struct
        for (unsigned int i = 0; i < shapes.size(); i++)
        {
            Geometry g;
            tinyobj::shape_t *shape = &shapes[i];
            tinyobj::mesh_t *mesh = &shapes[i].mesh;

            if (printDebug) std::cout << "Parsing: " << shape->name << std::endl;
            if (printDebug) std::cout << "Num Indices: " << mesh->indices.size() << std::endl;

            for (size_t i = 0; i < mesh->indices.size(); i += 3)
            {
                uint32_t idx1 = mesh->indices[i + 0];
                uint32_t idx2 = mesh->indices[i + 1];
                uint32_t idx3 = mesh->indices[i + 2];
                g.faces.push_back({ idx1, idx2, idx3 });
            }

            if (printDebug) std::cout << "Num TexCoords: " << mesh->texcoords.size() << std::endl;
            for (size_t i = 0; i < mesh->texcoords.size(); i += 2)
            {
                float uv1 = mesh->texcoords[i + 0];
                float uv2 = mesh->texcoords[i + 1];
                g.texCoords.push_back({ uv1, uv2 });
            }

            if (printDebug)  std::cout << mesh->positions.size() << " - " << mesh->texcoords.size() << std::endl;

            for (size_t v = 0; v < mesh->positions.size(); v += 3)
            {
                float3 vert = float3(mesh->positions[v + 0], mesh->positions[v + 1], mesh->positions[v + 2]);
                g.vertices.push_back(vert);
            }

            submesh.push_back(g);
        }

        for (auto m : materials)
        {
            if (m.diffuse_texname.size() <= 0) continue;
            std::string texName = parentDir + m.diffuse_texname;
            GlTexture2D tex = load_image(texName);
            mesh.textures.push_back(std::move(tex));
        }

        for (unsigned int i = 0; i < shapes.size(); i++)
        {
            auto g = submesh[i];
            g.compute_normals(false);
            mesh.chunks.push_back({ shapes[i].mesh.material_ids, make_mesh_from_geometry(g) });
        }

        return mesh;
    }

}

#pragma warning(pop)

#endif // gl_mesh_hpp
