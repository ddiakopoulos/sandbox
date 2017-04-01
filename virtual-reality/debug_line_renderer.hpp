#pragma once

#ifndef debug_renderer_hpp
#define debug_renderer_hpp

#include "linalg_util.hpp"
#include "GL_API.hpp"
#include "procedural_mesh.hpp"
#include "scene.hpp"

using namespace avl;

class DebugLineRenderer : public DebugRenderable
{
    struct Vertex { float3 position; float3 color; };
    std::vector<Vertex> vertices;
    GlMesh debugMesh;
    GlShader debugShader;

    Geometry axis = make_axis();
    Geometry box = make_cube();
    Geometry sphere = make_sphere(0.1f);

    constexpr static const char debugVertexShader[] = R"(#version 330 
        layout(location = 0) in vec3 v; 
        layout(location = 1) in vec3 c; 
        uniform mat4 u_mvp; 
        out vec3 oc; 
        void main() { gl_Position = u_mvp * vec4(v.xyz, 1); oc = c; }
    )";

    constexpr static const char debugFragmentShader[] = R"(#version 330 
        in vec3 oc; 
        out vec4 f_color; 
        void main() { f_color = vec4(oc.rgb, 1); }
    )";

public:

    DebugLineRenderer()
    {
        debugShader = GlShader(debugVertexShader, debugFragmentShader);
    }

    virtual void draw(const float4x4 & viewProj) override
    {
        debugMesh.set_vertices(vertices.size(), vertices.data(), GL_DYNAMIC_DRAW);
        debugMesh.set_attribute(0, &Vertex::position);
        debugMesh.set_attribute(1, &Vertex::color);
        debugMesh.set_non_indexed(GL_LINES);

        auto model = Identity4x4;
        auto modelViewProjectionMatrix = mul(viewProj, model);

        debugShader.bind();
        debugShader.uniform("u_mvp", modelViewProjectionMatrix);
        debugMesh.draw_elements();
        debugShader.unbind();
    }

    void clear()
    {
        vertices.clear();
    }

    // Coordinates should be provided pre-transformed to world-space
    void draw_line(const float3 & from, const float3 & to, const float3 color = float3(1, 1, 1))
    {
        vertices.push_back({ from, color });
        vertices.push_back({ to, color });
    }

    void draw_box(const Pose & pose, const float & half, const float3 color = float3(1, 1, 1))
    {
        // todo - apply exents
        for (const auto v : box.vertices)
        {
            auto tV = pose.transform_coord(v);
            vertices.push_back({ tV, color });
        }
    }

    void draw_box(const Bounds3D & bounds, const float3 color = float3(1, 1, 1))
    {
        std::cout << "Drawing: " << bounds << std::endl;

        for (float a : {bounds.min().x, bounds.max().x}) 
            for (float b : {bounds.min().y, bounds.max().y})
                for (float c : {bounds.min().z, bounds.max().z}) vertices.push_back({ {a, b, c}, color });

        for (float a : {bounds.min().x, bounds.max().x})
            for (float b : {bounds.min().y, bounds.max().y})
                for (float c : {bounds.min().z, bounds.max().z}) vertices.push_back({ {b, c, a}, color });

        for (float a : {bounds.min().x, bounds.max().x})
            for (float b : {bounds.min().y, bounds.max().y})
                for (float c : {bounds.min().z, bounds.max().z}) vertices.push_back({ {c, a, b}, color });
    }

    void draw_sphere(const Pose & pose, const float & radius, const float3 color = float3(1, 1, 1))
    {
        // todo - apply radius
        for (const auto v : sphere.vertices)
        {
            auto tV = pose.transform_coord(v);
            vertices.push_back({ tV, color });
        }
    }

    void draw_axis(const Pose & pose, const float3 color = float3(1, 1, 1))
    {
        for (int i = 0; i < axis.vertices.size(); ++i)
        {
            auto v = axis.vertices[i];
            v = pose.transform_coord(v);
            vertices.push_back({ v, axis.colors[i].xyz() });
        }
    }

    void draw_frustum(const float4x4 & view, const float3 color = float3(1, 1, 1))
    {
        Frustum f(view);
        for (auto p : f.get_corners()) vertices.push_back({ p, color });
    }

};

#endif // end debug_renderer_hpp
