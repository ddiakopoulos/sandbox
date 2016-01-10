#ifndef geometry_h
#define geometry_h

#include "linear_algebra.hpp"
#include "math_util.hpp"
#include "geometric.hpp"
#include "GlMesh.hpp"
#include "GlTexture.hpp"
#include "string_utils.hpp"

#include "tinyply.h"
#include "tiny_obj_loader.h"

#include <sstream>
#include <vector>
#include <fstream>
#include <algorithm>

using namespace math;
using namespace gfx;

namespace util
{
    struct TexturedMeshChunk
    {
        std::vector<int> materialIds;
        GlMesh mesh;
    };
    
    struct TexturedMesh
    {
        std::vector<TexturedMeshChunk> chunks;
        std::vector<GlTexture> textures;
    };
    
    struct Geometry
    {
        std::vector<math::float3> vertices;
        std::vector<math::float3> normals;
        std::vector<math::float4> colors;
        std::vector<math::float2> texCoords;
        std::vector<math::float3> tangents;
        std::vector<math::float3> bitangents;
        std::vector<math::uint3> faces;
        
        void compute_normals(bool smooth = true)
        {
            static const double NORMAL_EPSILON = 0.0001;
            
            normals.resize(vertices.size());
            
            for (auto & n : normals)
                n = math::float3(0,0,0);

            std::vector<uint32_t> uniqueVertIndices(vertices.size(), 0);
            if (smooth)
            {
                for (uint32_t i = 0; i < uniqueVertIndices.size(); ++i)
                {
                    if (uniqueVertIndices[i] == 0)
                    {
                        uniqueVertIndices[i] = i + 1;
                        const math::float3 v0 = vertices[i];
                        for (auto j = i + 1; j < vertices.size(); ++j)
                        {
                            const math::float3 v1 = vertices[j];
                            if (math::lengthSqr(v1 - v0) < NORMAL_EPSILON)
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
                
                const math::float3 v0 = vertices[idx0];
                const math::float3 v1 = vertices[idx1];
                const math::float3 v2 = vertices[idx2];
                
                math::float3 e0 = v1 - v0;
                math::float3 e1 = v2 - v0;
                math::float3 e2 = v2 - v1;
                
                if (lengthSqr(e0) < NORMAL_EPSILON) continue;
                if (lengthSqr(e1) < NORMAL_EPSILON) continue;
                if (lengthSqr(e2) < NORMAL_EPSILON) continue;
                
                math::float3 n = cross(e0, e1);
                
                n = normalize(n);
                
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
                n = math::normalize(n);
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
                math::uint3 face = faces[i];
                
                const math::float3 & v0 = vertices[face.x];
                const math::float3 & v1 = vertices[face.y];
                const math::float3 & v2 = vertices[face.z];
                
                const math::float2 & w0 = texCoords[face.x];
                const math::float2 & w1 = texCoords[face.y];
                const math::float2 & w2 = texCoords[face.z];
                
                // std::cout << "x: " << face.x << " y: " <<  face.y << " z: " << face.z << std::endl;
                
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
                math::float3 tangent((t2 * x1 - t1 * x2) * r, (t2 * y1 - t1 * y2) * r, (t2 * z1 - t1 * z2) * r);
                
                // Accumulate
                tangents[face.x] += tangent;
                tangents[face.y] += tangent;
                tangents[face.z] += tangent;
            }
            
            // Tangents
            for (size_t i = 0; i < vertices.size(); ++i)
            {
                const math::float3 normal = normals[i];
                const math::float3 tangent = tangents[i];
                
                // Gram-Schmidt orthogonalize
                tangents[i] = (tangent - normal * math::dot(normal, tangent));
                
                const float len = math::lengthL2(tangents[i]);
                
                if (len > 0.0f)
                    tangents[i] = tangents[i] / (float) sqrt(len);
            }
            
            // Bitangents 
            for (size_t i = 0; i < vertices.size(); ++i)
            {
                bitangents[i] = math::normalize(math::cross(normals[i], tangents[i]));
            }
        }
        
        math::Box<float, 3> compute_bounds() const
        {
            math::Box<float, 3> bounds;
            
            bounds.position = math::float3(std::numeric_limits<float>::infinity());
            bounds.dimensions = -bounds.position;
            
            for (const auto & vertex : vertices)
            {
                //auto newV = pose.transform_coord(vertex);
                bounds.position = min(bounds.position, vertex);
                bounds.dimensions = max(bounds.dimensions, vertex);
            }
            return bounds;
        }

    };
    
    inline gfx::GlMesh make_mesh_from_geometry(const Geometry & geometry)
    {
        gfx::GlMesh m;
        
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
    
    inline uint32_t make_vert(std::vector<std::tuple<math::float3, math::float2>> & buffer, const math::float3 & position, math::float2 texcoord)
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
            std::vector<math::float3> flatVerts;
            for (int i = 0; i < vertexCount * 3; i+=3)
                flatVerts.push_back(math::float3(verts[i], verts[i+1], verts[i+2]));
            
            geo.faces.reserve(numTriangles);
            std::vector<math::uint3> flatFaces;
            for (int i = 0; i < numTriangles * 3; i+=3)
                flatFaces.push_back(math::uint3(faces[i], faces[i+1], faces[i+2]));
            
            geo.texCoords.reserve(uvCount);
            std::vector<math::float2> flatTexCoords;
            for (int i = 0; i < uvCount * 6; i+=2)
               flatTexCoords.push_back(math::float2(texCoords[i], texCoords[i + 1]));
            
            std::vector<std::tuple<math::float3, math::float2>> uniqueVerts;
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
                for (auto v : flatVerts)
                {
                    geo.vertices.push_back(v);
                }
            }
            
            for (int i = 0; i < indexBuffer.size(); i+=3)
                geo.faces.push_back(math::uint3(indexBuffer[i], indexBuffer[i+1], indexBuffer[i+2]));

            geo.compute_normals();
            geo.compute_tangents();
            geo.compute_bounds();
            
        }
        catch (std::exception e)
        {
            ANVIL_ERROR("[tinyply] Caught exception:" << e.what());
        }
        
        return geo;
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
            GlTexture tex = load_image(texName);
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
    
    static const double SPHERE_EPSILON = 4.37114e-05;
    
    struct Sphere
    {
        math::float3 center;
        float radius;
        
        Sphere() {}
        Sphere(const math::float3 &  center, float radius) : center(center), radius(radius) {}
        
        // Returns the closest point on the ray to the Sphere. If ray intersects then returns the point of nearest intersection.
        math::float3 closest_point(const gfx::Ray & ray) const
        {
            float t;
            math::float3 diff = ray.get_origin() - center;
            float a = math::dot(ray.get_direction(), ray.get_direction());
            float b = 2.0f * math::dot(diff, ray.get_direction());
            float c = math::dot(diff, diff) - radius * radius;
            float disc = b * b - 4.0f * a * c;
            
            if (disc > 0)
            {
                float e = std::sqrt(disc);
                float denom = 2 * a;
                t = (-b - e) / denom;    // smaller root
                
                if (t > SPHERE_EPSILON) return ray.calculate_position(t);
                
                t = (-b + e) / denom;    // larger root
                if (t > SPHERE_EPSILON) return ray.calculate_position(t);
            }
            
            // doesn't intersect; closest point on line
            t = math::dot( -diff, math::normalize(ray.get_direction()) );
            math::float3 onRay = ray.calculate_position(t);
            return center + math::normalize( onRay - center ) * radius;
        }
        
        // Converts sphere to another coordinate system. Note that it will not return correct results if there are non-uniform scaling, shears, or other unusual transforms.
        Sphere transformed(const math::float4x4 & transform)
        {
            math::float4 tCenter = transform * math::float4(center, 1);
            math::float4 tRadius = transform * math::float4(radius, 0, 0, 0);
            return Sphere(math::float3(tCenter.x, tCenter.y, tCenter.z), length(tRadius));
        }
        
        void calculate_projection(float focalLength, math::float2 *outCenter, math::float2 *outAxisA, math::float2 *outAxisB) const
        {
            math::float3 o(-center.x, center.y, center.z);
            
            float r2 = radius * radius;
            float z2 = o.z * o.z;
            float l2 = math::dot(o, o);
            
            if (outCenter) *outCenter = focalLength * o.z * math::float2(o.x, o.y) / (z2-r2);
            
            if (fabs(z2 - l2) > 0.00001f)
            {
                if (outAxisA) *outAxisA = focalLength * sqrtf(-r2*(r2-l2)/((l2-z2)*(r2-z2)*(r2-z2))) * math::float2(o.x, o.y);
                if (outAxisB) *outAxisB = focalLength * sqrtf(fabs(-r2*(r2-l2)/((l2-z2)*(r2-z2)*(r2-l2)))) * math::float2(-o.y, o.x);
            }
            
            // approximate with circle
            else
            {
                float newRadius = focalLength * radius / sqrtf(z2 - r2);
                if (outAxisA) *outAxisA = math::float2(newRadius, 0);
                if (outAxisB) *outAxisB = math::float2(0, newRadius);
            }
        }
        
        // Calculates the projection of the sphere (an oriented ellipse) given a focal length. Algorithm due to Iñigo Quilez.
        void calculate_projection(float focalLength, math::float2 screenSizePixels, math::float2 * outCenter, math::float2 * outAxisA, math::float2 * outAxisB) const
        {
            auto toScreenPixels = [=] (math::float2 v, const math::float2 &winSizePx) {
                math::float2 result = v;
                result.x *= 1 / (winSizePx.x / winSizePx.y);
                result += math::float2(0.5f);
                result *= winSizePx;
                return result;
            };
            
            math::float2 center, axisA, axisB;
            
            calculate_projection(focalLength, &center, &axisA, &axisB);
            if (outCenter) *outCenter = toScreenPixels(center, screenSizePixels);
            if (outAxisA) *outAxisA = toScreenPixels(center + axisA * 0.5f, screenSizePixels) - toScreenPixels(center - axisA * 0.5f, screenSizePixels);
            if (outAxisB) *outAxisB = toScreenPixels(center + axisB * 0.5f, screenSizePixels) - toScreenPixels(center - axisB * 0.5f, screenSizePixels);
        }
        
    };
    
    static const double PLANE_EPSILON = 0.0001;
    
    struct Plane
    {
        math::float4 equation; // ax * by * cz + d form (xyz normal, w distance)
        Plane(const math::float4 & equation) : equation(equation) {}
        Plane(const math::float3 & normal, const float & distance) { equation = math::float4(normal.x, normal.y, normal.z, distance); }
        Plane(const math::float3 & normal, const math::float3 & point) { equation = math::float4(normal.x, normal.y, normal.z, -dot(normal, point)); }
        math::float3 get_normal() const { return math::float3(equation.x, equation.y, equation.z); }
        void normalize() { float n = 1.0f / math::length(get_normal()); equation *= n; };
        float get_distance() const { return equation.w; }
        float distance_to(math::float3 point) const { return dot(get_normal(), point) + equation.w; };
        bool contains(math::float3 point) const { return std::abs(distance_to(point)) < PLANE_EPSILON; };
    };
    
    struct Segment
    {
        math::float3 first, second;
        Segment(math::float3 first, math::float3 second) : first(first), second(second) {}
        math::float3 get_direction() const { return normalize (second - first); };
    };
    
    //////////////////////////////
    // Ray-object intersections //
    //////////////////////////////
    
    // The point where the line p0-p2 intersects the plane n&d
    inline math::float3 plane_line_intersection(const math::float3 & n, const float d, const math::float3 & p0, const math::float3 & p1)
    {
        math::float3 dif = p1 - p0;
        float dn = math::dot(n, dif);
        float t = -(d + math::dot(n, p0)) / dn;
        return p0 + (dif*t);
    }
    
    // The point where the line p0-p2 intersects the plane n&d
    inline math::float3 plane_line_intersection(const math::float4 & plane, const math::float3 & p0, const math::float3 & p1)
    {
        return plane_line_intersection(plane.xyz(), plane.w, p0, p1);
    }
    
    inline bool intersect_ray_plane(const gfx::Ray & ray, const Plane & p, math::float3 * intersection, float * outT = nullptr)
    {

        float d = math::dot(ray.direction, p.get_normal());
        // Make sure we're not parallel to the plane
        if (std::abs(d) > PLANE_EPSILON)
        {
            float t = -p.distance_to(ray.origin) / d;
            
            if (t >= 0.0f)
            {
                if (outT) *outT = t;
                if (intersection) *intersection = ray.origin + t * ray.direction;
                return true;
            }
        }
        return false;
    }
    
    // Implementation adapted from: http://www.lighthouse3d.com/tutorials/maths/ray-triangle-intersection/
    inline bool intersect_ray_triangle(const gfx::Ray & ray, const math::float3 & v0, const math::float3 & v1, const math::float3 & v2, float * outT, math::float2 * outUV = nullptr)
    {
        math::float3 e1 = v1 - v0, e2 = v2 - v0, h = cross(ray.direction, e2);
        
        float a = dot(e1, h);
        if (fabsf(a) == 0.0f) return false; // Ray is collinear with triangle plane
        
        math::float3 s = ray.origin - v0;
        float f = 1 / a;
        float u = f * dot(s,h);
        if (u < 0 || u > 1) return false; // Line intersection is outside bounds of triangle
        
        math::float3 q = cross(s, e1);
        float v = f * math::dot(ray.direction, q);
        if (v < 0 || u + v > 1) return false; // Line intersection is outside bounds of triangle
        
        float t = f * dot(e2, q);
        if (t < 0) return false; // Line intersection, but not a ray intersection
        
        if (outT) *outT = t;
        if (outUV) *outUV = {u,v};
        
        return true;
    }
    
     // Real-Time Collision Detection pg. 180
    inline bool intersect_ray_box(const gfx::Ray & ray, const math::Box<float, 3> bounds, float *outT = nullptr)
    {
        float tmin = 0.0f; // set to -FLT_MAX to get first hit on line
        float tmax = std::numeric_limits<float>::max(); // set to max distance ray can travel (for segment)
        
        // For all three slabs
        for (int i = 0; i < 3; i++)
        {
            if (std::abs(ray.direction[i]) < PLANE_EPSILON)
            {
                // Ray is parallel to slab. No hit if r.origin not within slab
                if ((ray.origin[i] < bounds.position[i]) || (ray.origin[i] > bounds.dimensions[i]))
                {
                    return false;
                }
            }
            else
            {
                // Compute intersection t value of ray with near and far plane of slab
                float ood(1.0f / ray.direction[i]);
                float t1((bounds.position[i] - ray.origin[i]) * ood);
                float t2((bounds.dimensions[i] - ray.origin[i]) * ood);
                
                // Make t1 be intersection with near plane, t2 with far plane
                if (t1 > t2)
                {
                    float tmp = t1;
                    t1 = t2;
                    t2 = tmp;
                }
                
                // Compute the intersection of slab intersection intervals
                tmin = std::max<float>(tmin, t1); // Rather than: if (t1 > tmin) tmin = t1;
                tmax = std::min<float>(tmax, t2); // Rather than: if (t2 < tmax) tmax = t2;
                
                // Exit with no collision as soon as slab intersection becomes empty
                if (tmin > tmax)
                {
                    return false;
                }
            }
        }
        
        // Ray intersects all 3 slabs. Intersection t value (tmin)
        if (outT) *outT = tmin;
        
        return true;
    }
    
    inline bool intersect_ray_box(const gfx::Ray & ray, const math::float3 & boxMin, const math::float3 & boxMax)
    {
        // Determine an interval t0 <= t <= t1 in which ray(t).x is within the box extents
        float t0 = (boxMin.x - ray.origin.x) / ray.direction.x, t1 = (boxMax.x - ray.origin.x) / ray.direction.x;
        if(ray.direction.x < 0) std::swap(t0, t1);
        
        // Determine an interval t0y <= t <= t1y in which ray(t).y is within the box extents
        float t0y = (boxMin.y - ray.origin.y) / ray.direction.y, t1y = (boxMax.y - ray.origin.y) / ray.direction.y;
        if(ray.direction.y < 0) std::swap(t0y, t1y);
        
        // Intersect this interval with the previously computed interval
        if (t0 > t1y || t0y > t1) return false;
        t0 = std::max(t0, t0y);
        t1 = std::min(t1, t1y);
        
        // Determine an interval t0z <= t <= t1z in which ray(t).z is within the box extents
        float t0z = (boxMin.z - ray.origin.z) / ray.direction.z, t1z = (boxMax.z - ray.origin.z) / ray.direction.z;
        if(ray.direction.z < 0) std::swap(t0z, t1z);
        
        // Intersect this interval with the previously computed interval
        if (t0 > t1z || t0z > t1) return false;
        t0 = std::max(t0, t0z);
        t1 = std::min(t1, t1z);
        
        // True if the intersection interval is in front of the ray
        return t0 > 0;
    }
    
    inline bool intersect_ray_sphere(const gfx::Ray & ray, const Sphere & sphere, float * intersection = nullptr)
    {
        float t;
        math::float3 diff = ray.get_origin() - sphere.center;
        float a = math::dot(ray.get_direction(), ray.get_direction());
        float b = 2.0f * math::dot(diff, ray.get_direction());
        float c = math::dot(diff, diff) - sphere.radius * sphere.radius;
        float disc = b * b - 4.0f * a * c;
        
        if (disc < 0.0f) return false;
        else
        {
            float e = std::sqrt(disc);
            float denom = 2.0f * a;
            t = (-b - e) / denom;
            
            if (t > SPHERE_EPSILON)
            {
                if (intersection) *intersection = t;
                return true;
            }
            
            t = (-b + e) / denom;
            if (t > SPHERE_EPSILON)
            {
                if (intersection) *intersection = t;
                return true;
            }
        }
        
        if (intersection) *intersection = 0;
        return false;
    }
    
    // todo - impelement method for GlMesh as well
    inline bool intersect_ray_mesh(const gfx::Ray & ray, const Geometry & mesh, float * outRayT)
    {
        float bestT = std::numeric_limits<float>::infinity(), t;
        math::float2 outUv;
        
        auto meshBounds = mesh.compute_bounds();
        if (meshBounds.contains(ray.origin) || intersect_ray_box(ray, meshBounds.position, meshBounds.dimensions))
        {
            for (auto & tri : mesh.faces)
            {
                if (intersect_ray_triangle(ray, mesh.vertices[tri.x], mesh.vertices[tri.y], mesh.vertices[tri.z], &t, &outUv) && t < bestT)
                    bestT = t;
            }
        }
        
        if (bestT == std::numeric_limits<float>::infinity())
            return false;
        
        if (outRayT)
            *outRayT = bestT;
        
        return true;
    }

    
} // end namespace util

#endif // geometry_h
