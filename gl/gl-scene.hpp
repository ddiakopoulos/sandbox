#pragma once

#ifndef scene_hpp
#define scene_hpp

#include "gl-api.hpp"
#include "gl-mesh.hpp"
#include "gl-camera.hpp"

namespace avl
{
 
struct FogShaderParams
{
    GlTexture2D gradientTex;

    float startDistance = 0.0f;
    float endDistance = 64.0f;
    int textureWidth = 32;

    float heightFogThickness = 1.15f;
    float heightFogFalloff = 0.1f;
    float heightFogBaseHeight = -16.0f;

    float3 color = {1, 1, 1};

    void set_uniforms(GlShader & prog)
    {
        if (!gradientTex) generate_gradient_tex();

        float scale = 1.0f / (endDistance - startDistance);
        float add = -startDistance / (endDistance - startDistance);

        prog.bind();
        prog.uniform("u_gradientFogScaleAdd", float2(scale, add));
        prog.uniform("u_gradientFogLimitColor", float3(1, 1, 1));
        prog.uniform("u_heightFogParams", float3(heightFogThickness, heightFogFalloff, heightFogBaseHeight));
        prog.uniform("u_heightFogColor", color); // use color above
        prog.texture("s_gradientFogTexture", 0, gradientTex, GL_TEXTURE_2D); 
        prog.unbind();
    }

    void generate_gradient_tex()
    {
        glBindTexture(GL_TEXTURE_2D, gradientTex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        std::vector<uint4> gradient(textureWidth);
        auto gradientFunc = [](float t) { return 0.0 * (1 - t) + 1.0 * t; };

        float ds = 1.0f / (textureWidth - 1);
        float s = 0.0f;
        for (int i = 0; i < textureWidth; i++)
        {
            const uint8_t g = gradientFunc(s) * 255;
            gradient[i] = uint4(255, g, g, 255);
            //std::cout << gradient[i] << std::endl;
            s += ds;
        }

       gradientTex.setup(textureWidth, 1, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, gradient.data());
    }
};

struct ViewportRaycast
{
    GlCamera & cam; float2 viewport;
    ViewportRaycast(GlCamera & camera, float2 viewport) : cam(camera), viewport(viewport) {}
    Ray from(float2 cursor) { return cam.get_world_ray(cursor, viewport); };
};

struct RaycastResult
{
    bool hit = false;
    float distance = std::numeric_limits<float>::max();
    float3 normal = {0, 0, 0};
    RaycastResult(bool h, float t, float3 n) : hit(h), distance(t), normal(n) {}
};

struct GameObject
{
    virtual ~GameObject() {}
    virtual void update(const float & dt) {}
    virtual void draw() const {};
    virtual Bounds3D get_world_bounds() const = 0;
    virtual Bounds3D get_bounds() const = 0;
    virtual float3 get_scale() const = 0;
    virtual void set_scale(const float3 & s) = 0;
    virtual Pose get_pose() const = 0;
    virtual void set_pose(const Pose & p) = 0;
    virtual RaycastResult raycast(const Ray & worldRay) const = 0;
};

struct DebugRenderable
{
    virtual void draw(const float4x4 & viewProj) = 0;
};

class SimpleStaticMesh
{
    Pose pose;
    float3 scale{ 1, 1, 1 };
    GlMesh mesh;
    Geometry geom;
    Bounds3D bounds;

public:

    SimpleStaticMesh() {}

    Pose get_pose() const { return pose; }
    void set_pose(const Pose & p) { pose = p; }
    float3 get_scale() const { return scale; }
    void set_scale(const float3 & s) { scale = s; }
    Bounds3D get_bounds() const { return bounds; }
    Geometry & get_geometry() { return geom; }
    void draw() const { mesh.draw_elements(); }
    void update(const float & dt) { }

    Bounds3D get_world_bounds() const
    {
        const Bounds3D local = get_bounds();
        const float3 scale = get_scale();
        return{ pose.transform_coord(local.min()) * scale, pose.transform_coord(local.max()) * scale };
    }

    RaycastResult raycast(const Ray & worldRay) const
    {
        auto localRay = pose.inverse() * worldRay;
        localRay.origin /= scale;
        localRay.direction /= scale;
        float outT = 0.0f;
        float3 outNormal = { 0, 0, 0 };
        bool hit = intersect_ray_mesh(localRay, geom, &outT, &outNormal);
        return{ hit, outT, outNormal };
    }

    void set_static_mesh(const Geometry & g, const float scale = 1.f, const GLenum usage = GL_STATIC_DRAW)
    {
        geom = g;
        if (scale != 1.f) rescale_geometry(geom, scale);
        bounds = geom.compute_bounds();
        mesh = make_mesh_from_geometry(geom, usage);
    }

    void set_mesh_render_mode(GLenum renderMode)
    {
        if (renderMode != GL_TRIANGLE_STRIP) mesh.set_non_indexed(renderMode);
    }
};

} // end namespace avl

#endif
