#ifndef geometry_h
#define geometry_h

#include "linalg_util.hpp"
#include "math_util.hpp"
#include "geometric.hpp"
#include "string_utils.hpp"
#include "GL_API.hpp"

#include "tinyply.h"
#include "tiny_obj_loader.h"

#include <sstream>
#include <vector>
#include <fstream>
#include <algorithm>

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
    
    struct Geometry
    {
        std::vector<float3> vertices;
        std::vector<float3> normals;
        std::vector<float4> colors;
        std::vector<float2> texCoords;
        std::vector<float3> tangents;
        std::vector<float3> bitangents;
        std::vector<uint3> faces;
        
        void compute_normals(bool smooth = true)
        {
            static const double NORMAL_EPSILON = 0.0001;
            
            normals.resize(vertices.size());
            
            for (auto & n : normals)
                n = float3(0,0,0);

            std::vector<uint32_t> uniqueVertIndices(vertices.size(), 0);
            if (smooth)
            {
                for (uint32_t i = 0; i < uniqueVertIndices.size(); ++i)
                {
                    if (uniqueVertIndices[i] == 0)
                    {
                        uniqueVertIndices[i] = i + 1;
                        const float3 v0 = vertices[i];
                        for (auto j = i + 1; j < vertices.size(); ++j)
                        {
                            const float3 v1 = vertices[j];
                            if (length2(v1 - v0) < NORMAL_EPSILON)
                            {
                                uniqueVertIndices[j] = uniqueVertIndices[i];
                            }
                        }
                    }
                }
            }
            
            uint32_t idx0, idx1, idx2;
            for (size_t i = 0; i < faces.size(); ++i)
            {
                auto f = faces[i];

                idx0 = (smooth) ? uniqueVertIndices[f.x] - 1 : f.x;
                idx1 = (smooth) ? uniqueVertIndices[f.y] - 1 : f.y;
                idx2 = (smooth) ? uniqueVertIndices[f.z] - 1 : f.z;
                
                const float3 v0 = vertices[idx0];
                const float3 v1 = vertices[idx1];
                const float3 v2 = vertices[idx2];
                
                float3 e0 = v1 - v0;
                float3 e1 = v2 - v0;
                float3 e2 = v2 - v1;
                
                if (length2(e0) < NORMAL_EPSILON) continue;
                if (length2(e1) < NORMAL_EPSILON) continue;
                if (length2(e2) < NORMAL_EPSILON) continue;
                
                float3 n = cross(e0, e1);
                
                n = safe_normalize(n);
                
                normals[f.x] += n;
                normals[f.y] += n;
                normals[f.z] += n;
                
                // Copy normals for non-unique verts
                if (smooth)
                {
                    for (uint32_t i = 0; i < vertices.size(); ++i)
                    {
                        normals[i] = normals[uniqueVertIndices[i]-1];
                    }
                }
            }
            
            for (auto & n : normals)
                n = safe_normalize(n);
        }
        
        // Lengyel, Eric. "Computing Tangent Space Basis Vectors for an Arbitrary Mesh".
        // Terathon Software 3D Graphics Library, 2001.
        void compute_tangents()
        {
            //assert(normals.size() == vertices.size());
            
            tangents.resize(vertices.size());
            bitangents.resize(vertices.size());
            
            // Each face
            for (size_t i = 0; i < faces.size(); ++i)
            {
                uint3 face = faces[i];
                
                const float3 & v0 = vertices[face.x];
                const float3 & v1 = vertices[face.y];
                const float3 & v2 = vertices[face.z];
                
                const float2 & w0 = texCoords[face.x];
                const float2 & w1 = texCoords[face.y];
                const float2 & w2 = texCoords[face.z];
                
                //std::cout << "x: " << face.x << " y: " <<  face.y << " z: " << face.z << std::endl;
                
                float x1 = v1.x - v0.x;
                float x2 = v2.x - v0.x;
                float y1 = v1.y - v0.y;
                float y2 = v2.y - v0.y;
                float z1 = v1.z - v0.z;
                float z2 = v2.z - v0.z;
                
                float s1 = w1.x - w0.x;
                float s2 = w2.x - w0.x;
                
                float t1 = w1.y - w0.y;
                float t2 = w2.y - w0.y;
                
                float r = (s1 * t2 - s2 * t1);
                
                if (r != 0.0f)
                    r = 1.0f / r;
                
                // Tangent in the S direction
                float3 tangent((t2 * x1 - t1 * x2) * r, (t2 * y1 - t1 * y2) * r, (t2 * z1 - t1 * z2) * r);
                
                // Accumulate
                tangents[face.x] += tangent;
                tangents[face.y] += tangent;
                tangents[face.z] += tangent;
            }
            
            // Tangents
            for (size_t i = 0; i < vertices.size(); ++i)
            {
                const float3 normal = normals[i];
                const float3 tangent = tangents[i];
                
                // Gram-Schmidt orthogonalize
                tangents[i] = (tangent - normal * dot(normal, tangent));
                
                const float len = length(tangents[i]);
                
                if (len > 0.0f)
                    tangents[i] = tangents[i] / (float) sqrt(len);
            }
            
            // Bitangents 
            for (size_t i = 0; i < vertices.size(); ++i)
            {
                bitangents[i] = safe_normalize(cross(normals[i], tangents[i]));
            }
        }
        
        Bounds3D compute_bounds() const
        {
            Bounds3D bounds;
            
            bounds._min = float3(std::numeric_limits<float>::infinity());
            bounds._max = -bounds.min();
            
            for (const auto & vertex : vertices)
            {
                bounds._min = linalg::min(bounds.min(), vertex);
                bounds._max = linalg::max(bounds.max(), vertex);
            }
            return bounds;
        }

    };
    
    inline void rescale_geometry(Geometry & g, float radius = 1.0f)
    {
        auto bounds = g.compute_bounds();
        
        float3 r = bounds.size() * 0.5f;
        float3 center = bounds.center();
        
        float oldRadius = std::max(r.x, std::max(r.y, r.z));
        float scale = radius / oldRadius;
        
        for (auto & v : g.vertices)
        {
            float3 fixed = scale * (v - center);
            v = fixed;
        }
    }
    
    inline GlMesh make_mesh_from_geometry(const Geometry & geometry)
    {
        GlMesh m;
        
        int vertexOffset = 0;
        int normalOffset = 0;
        int colorOffset = 0;
        int texOffset = 0;
        int tanOffset = 0;
        int bitanOffset = 0;
        
        int components = 3;
        
        if (geometry.normals.size() != 0 )
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
        m.set_vertex_data(buffer.size() * sizeof(float), buffer.data(), GL_STATIC_DRAW);
        m.set_attribute(0, 3, GL_FLOAT, GL_FALSE, components * sizeof(float), ((float*) 0) + vertexOffset);
        if (normalOffset) m.set_attribute(1, 3, GL_FLOAT, GL_FALSE, components * sizeof(float), ((float*) 0) + normalOffset);
        if (colorOffset) m.set_attribute(2, 3, GL_FLOAT, GL_FALSE, components * sizeof(float), ((float*) 0) + colorOffset);
        if (texOffset) m.set_attribute(3, 2, GL_FLOAT, GL_FALSE, components * sizeof(float), ((float*) 0) + texOffset);
        if (tanOffset) m.set_attribute(4, 3, GL_FLOAT, GL_FALSE, components * sizeof(float), ((float*) 0) + tanOffset);
        if (bitanOffset) m.set_attribute(5, 3, GL_FLOAT, GL_FALSE, components * sizeof(float), ((float*) 0) + bitanOffset);
        
        if (geometry.faces.size() > 0)
            m.set_elements(geometry.faces, GL_STATIC_DRAW);
        
        return m;
    }
    
    inline uint32_t make_vert(std::vector<std::tuple<float3, float2>> & buffer, const float3 & position, float2 texcoord)
    {
        auto vert = std::make_tuple(position, texcoord);
        auto it = std::find(begin(buffer), end(buffer), vert);
        if(it != end(buffer)) return it - begin(buffer);
        buffer.push_back(vert); // Add to unique list if we didn't find it
        return (uint32_t) buffer.size() - 1;
    }
    
    // Handles trimeshes only
    inline Geometry load_geometry_from_ply(const std::string & path)
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
            
            uint32_t vertexCount = file.request_properties_from_element("vertex", {"x", "y", "z"}, verts);
            uint32_t numTriangles = file.request_properties_from_element("face", {"vertex_indices"}, faces, 3);
            uint32_t uvCount = (hasTexcoords) ? file.request_properties_from_element("face", {"texcoord"}, texCoords, 6) : 0;
            
            file.read(ss);
            
            geo.vertices.reserve(vertexCount);
            std::vector<float3> flatVerts;
            for (uint32_t i = 0; i < vertexCount * 3; i+=3)
                flatVerts.push_back(float3(verts[i], verts[i+1], verts[i+2]));
            
            geo.faces.reserve(numTriangles);
            std::vector<uint3> flatFaces;
            for (uint32_t i = 0; i < numTriangles * 3; i+=3)
                flatFaces.push_back(uint3(faces[i], faces[i+1], faces[i+2]));
            
            geo.texCoords.reserve(uvCount);
            std::vector<float2> flatTexCoords;
            for (uint32_t i = 0; i < uvCount * 6; i+=2)
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
                for (int i = 0; i < indexBuffer.size(); i+=3) geo.faces.push_back(uint3(indexBuffer[i], indexBuffer[i+1], indexBuffer[i+2]));
            }
            else geo.faces = flatFaces;

            geo.compute_normals(false);
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

        if (status && !err.empty())
            std::cerr << "tinyobj sys: " + err << std::endl;
        
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
                g.faces.push_back({idx1, idx2, idx3});
            }
            for (size_t i = 0; i < mesh->texcoords.size(); i+=2)
            {
                float uv1 = mesh->texcoords[i + 0];
                float uv2 = mesh->texcoords[i + 1];
                g.texCoords.push_back({uv1, uv2});
            }
            
            for (size_t v = 0; v < mesh->positions.size(); v += 3)
            {
                float3 vert = float3(mesh->positions[v + 0], mesh->positions[v + 1], mesh->positions[v + 2]);
                g.vertices.push_back(vert);
            }
            
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
                g.faces.push_back({idx1, idx2, idx3});
            }
            
            if (printDebug) std::cout << "Num TexCoords: " << mesh->texcoords.size() << std::endl;
            for (size_t i = 0; i < mesh->texcoords.size(); i+=2)
            {
                float uv1 = mesh->texcoords[i + 0];
                float uv2 = mesh->texcoords[i + 1];
                g.texCoords.push_back({uv1, uv2});
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
            mesh.chunks.push_back({shapes[i].mesh.material_ids, make_mesh_from_geometry(g)});
        }
        
        return mesh;
    }
    
    inline Geometry concatenate_geometry(const Geometry & a, const Geometry & b)
    {
        Geometry s;
        s.vertices.insert(s.vertices.end(), a.vertices.begin(), a.vertices.end());
        s.vertices.insert(s.vertices.end(), b.vertices.begin(), b.vertices.end());
        s.faces.insert(s.faces.end(), a.faces.begin(), a.faces.end());
        for (auto & f : b.faces) s.faces.push_back({ (int) a.vertices.size() + f.x, (int) a.vertices.size() + f.y, (int) a.vertices.size() + f.z} );
        return s;
    }

    inline bool intersect_ray_mesh(const Ray & ray, const Geometry & mesh, float * outRayT = nullptr, float3 * outFaceNormal = nullptr, Bounds3D * bounds = nullptr)
    {
        float bestT = std::numeric_limits<float>::infinity(), t;
        uint3 bestFace = {0, 0, 0};
        float2 outUv;

        Bounds3D meshBounds = (bounds) ? *bounds : mesh.compute_bounds(); 
        if (!meshBounds.contains(ray.origin) && intersect_ray_box(ray, meshBounds))
        {
            for (int f = 0; f < mesh.faces.size(); ++f)
            {
                auto & tri = mesh.faces[f];
                if (intersect_ray_triangle(ray, mesh.vertices[tri.x], mesh.vertices[tri.y], mesh.vertices[tri.z], &t, &outUv) && t < bestT)
                {
                    bestT = t;
                    bestFace = mesh.faces[f];
                }
            }
        }
        
        if (bestT == std::numeric_limits<float>::infinity()) return false;
        
        if (outRayT) *outRayT = bestT;
        
        if (outFaceNormal)
        {
            auto v0 = mesh.vertices[bestFace.x];
            auto v1 = mesh.vertices[bestFace.y];
            auto v2 = mesh.vertices[bestFace.z];
            float3 n = safe_normalize(cross(v1 - v0, v2 - v0));
            *outFaceNormal = n;
        }
        
        return true;
    }

}

#endif // geometry_h
