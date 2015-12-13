// See COPYING file for attribution information

#pragma once

#ifndef meshline_h
#define meshline_h

#include "GlShared.hpp"
#include "GlMesh.hpp"
#include "constant_spline.hpp"
#include <random>

class MeshLine
{
    std::random_device rd;
    std::mt19937 gen;

    gfx::GlShader shader;
    gfx::GlMesh mesh;
    gfx::GlCamera & camera;
    
    float2 screenDims;
    float linewidth;
    float3 color;
    
    std::vector<float3> previous;
    std::vector<float3> next;
    std::vector<float> side;
    std::vector<float> width;
    std::vector<float2> uvs;
    std::vector<uint3> indices;
    
    GlMesh make_line_mesh(const std::vector<float3> & curve)
    {
        GlMesh m;
        
        int components = 3 + 3 + 3 + 1 + 1 + 2;
        std::vector<float> buffer;
        for (size_t i = 0; i < curve.size(); ++i)
        {
            buffer.push_back(curve[i].x);
            buffer.push_back(curve[i].y);
            buffer.push_back(curve[i].z);
            buffer.push_back(previous[i].x);
            buffer.push_back(previous[i].y);
            buffer.push_back(previous[i].z);
            buffer.push_back(next[i].x);
            buffer.push_back(next[i].y);
            buffer.push_back(next[i].z);
            buffer.push_back(side[i]);
            buffer.push_back(width[i]);
            buffer.push_back(uvs[i].x);
            buffer.push_back(uvs[i].y);
        }
        
        m.set_vertex_data(buffer.size() * sizeof(float), buffer.data(), GL_STATIC_DRAW);
        m.set_attribute(0, 3, GL_FLOAT, GL_FALSE, components * sizeof(float), ((float*) 0) + 0); // Positions
        m.set_attribute(1, 3, GL_FLOAT, GL_FALSE, components * sizeof(float), ((float*) 0) + 3); // Previous
        m.set_attribute(2, 3, GL_FLOAT, GL_FALSE, components * sizeof(float), ((float*) 0) + 6); // Next
        m.set_attribute(3, 1, GL_FLOAT, GL_FALSE, components * sizeof(float), ((float*) 0) + 9); // Side
        m.set_attribute(4, 1, GL_FLOAT, GL_FALSE, components * sizeof(float), ((float*) 0) + 10); // Width
        m.set_attribute(5, 2, GL_FLOAT, GL_FALSE, components * sizeof(float), ((float*) 0) + 11); // Uvs
        
        if (indices.size() > 0)
            m.set_elements(indices, GL_STATIC_DRAW);
        
        return m;
    }

public:

    MeshLine(gfx::GlCamera & camera, const float2 screenDims, const float linewidth, const float3 color) : camera(camera), screenDims(screenDims), linewidth(linewidth), color(color)
    {
        gen = std::mt19937(rd());
        shader = gfx::GlShader(read_file_text("assets/shaders/meshline_vert.glsl"), read_file_text("assets/shaders/meshline_frag.glsl"));
        auto line = create_curve();
        process(line);
        mesh = make_line_mesh(line);
    }
    
    void process(const std::vector<float3> & curve)
    {
        const auto l = curve.size();
        
        for (size_t i = 0; i < l; i += 2)
        {
            side.push_back(1.f);
            side.push_back(-1.f);
        }
        
        for (size_t i = 0; i < l; i += 2)
        {
            width.push_back(1.f);
            width.push_back(1.f);
        }
        
        for (size_t i = 0; i < l; i += 2)
        {
            uvs.push_back(float2(float(i) / (l - 1), 0.f));
            uvs.push_back(float2(float(i) / (l - 1), 1.f));
        }
        
        float3 v;
        
        {
            if (curve[0] == curve[l - 1]) v = curve[l - 2];
            else v = curve[0];
            previous.push_back(v);
            previous.push_back(v);
        }
        
        for (size_t i = 0; i < l - 1; i += 2)
        {
            v = curve[i];
            previous.push_back(v);
            previous.push_back(v);
        }
        
        for (size_t i = 2; i < l; i += 2)
        {
            v = curve[i];
            next.push_back(v);
            next.push_back(v);
        }
        
        {
            if (curve[l - 1] == curve[0]) v = curve[1];
            else v = curve[l - 1];
            next.push_back(v);
            next.push_back(v);
        }
        
        for (size_t i = 0; i < l - 1; i ++)
        {
            uint32_t n = i * 2;
            indices.push_back(uint3(n + 0, n + 1, n + 2));
            indices.push_back(uint3(n + 2, n + 1, n + 3));
        }
    }
    
    std::vector<float3> create_curve(float rMin = 3.f, float rMax = 12.f)
    {
        std::vector<float3> curve;

        auto r = std::uniform_real_distribution<float>(0.0, 1.0);

        ConstantSpline s;
        
        s.p0 = float3(0, 0, 0);
        s.p1 = s.p0 + float3( .5f - r(gen), .5f - r(gen), .5f - r(gen));
        s.p2 = s.p1 + float3( .5f - r(gen), .5f - r(gen), .5f - r(gen));
        s.p3 = s.p2 + float3( .5f - r(gen), .5f - r(gen), .5f - r(gen));
        
        s.p0 *= rMin + r(gen) * rMax;
        s.p1 *= rMin + r(gen) * rMax;
        s.p2 *= rMin + r(gen) * rMax;
        s.p3 *= rMin + r(gen) * rMax;
        
        s.calculate(.001f);
        s.calculate_distances();
        s.reticulate(256);
        
        auto sPoints = s.get_spline();
        
        for (const auto & p : sPoints)
        {
            curve.push_back(p);
            curve.push_back(p);
        }
        
        return curve;
    }
    
    void draw()
    {
        shader.bind();
        
        float4x4 model = Identity4x4;
        camera.nearClip = 0.1;
        camera.farClip = 64;
        auto projMat = camera.get_projection_matrix(screenDims.x / screenDims.y);
        auto viewMat = camera.get_view_matrix();
        
        shader.uniform("u_projMat", projMat);
        shader.uniform("u_modelViewMat", viewMat * model);
        
        shader.uniform("resolution", screenDims);
        shader.uniform("lineWidth", 24.0f);
        shader.uniform("color", color);
        shader.uniform("opacity", 1.0f);
        shader.uniform("near", camera.nearClip);
        shader.uniform("far", camera.farClip);
        shader.uniform("sizeAttenuation", 0.0f);
        shader.uniform("useMap", 0.0f);
        
        mesh.draw_elements();
        
        shader.unbind();
    }

};

#endif // meshline_h
