#pragma once

#ifndef scene_hpp
#define scene_hpp

#include "GL_API.hpp"
#include "geometry.hpp"
#include "camera.hpp"

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
            const auto g = gradientFunc(s) * 255;
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
    
struct Object
{
    Pose pose;
    float3 scale;
    Object() : scale(1, 1, 1) {}
    float4x4 get_model() const { return mul(pose.matrix(), make_scaling_matrix(scale)); }
    Bounds3D bounds;
};

struct Renderable : public Object
{
    GlMesh mesh;
    Geometry geom;
    bool castsShadow;
    bool isEmissive = false;
        
    Renderable() {}

    Renderable(const Geometry & g, bool shadow = true, GLenum renderMode = GL_TRIANGLE_STRIP) : geom(g), castsShadow(shadow)
    {
        rebuild_mesh();
		if (renderMode != GL_TRIANGLE_STRIP)
		{
			mesh.set_non_indexed(renderMode);
			glPointSize(5);
		}
    }
        
    void rebuild_mesh() 
	{ 
		bounds = geom.compute_bounds(); 
		mesh = make_mesh_from_geometry(geom); 
	}
        
    void draw() const 
	{ 
		mesh.draw_elements(); 
	};
        
    RaycastResult check_hit(const Ray & worldRay) const
    {
        auto localRay = pose.inverse() * worldRay;
        localRay.origin /= scale;
        localRay.direction /= scale;
        float outT = 0.0f;
        float3 outNormal = {0, 0, 0};
        bool hit = intersect_ray_mesh(localRay, geom, &outT, &outNormal);
        return {hit, outT, outNormal};
    }
};

} // end namespace avl

#endif