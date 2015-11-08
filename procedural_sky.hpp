#ifndef procedural_sky_h
#define procedural_sky_h

#include "linear_algebra.hpp"
#include "math_util.hpp"
#include "util.hpp"
#include "GlShared.hpp"
#include "file_io.hpp"

#include "hosek.hpp"
#include "preetham.hpp"
#include "procedural_mesh.hpp"

// http://www.learnopengl.com/#!Advanced-Lighting/HDR

class ProceduralSky
{

protected:
    
    GlMesh skyMesh;
    float sunTheta;
    float turbidity;
    float albedo;
    float normalizedSunY;
    bool shouldRecomputeMode = true;
    
    virtual void render_internal(float4x4 viewProj, float3 sunDir, float4x4 world) = 0;
    
public:
    
    float sunPhi = 230; // 0 - 360
    
    ProceduralSky(float sunTheta = 80, float turbidity = 4, float albedo = 0.1, float normalizedSunY = 1.15)
    {
        skyMesh = make_sphere_mesh(1.0);
        this->sunTheta = sunTheta;
        this->turbidity = turbidity;
        this->albedo = albedo;
        this->normalizedSunY = normalizedSunY;
    }

    void render(float4x4 viewProj, float3 eyepoint, float farClip)
    {
        glDisable(GL_BLEND);
        glDisable(GL_CULL_FACE);

        float3 sunDirection = spherical(to_radians(sunTheta), to_radians(sunPhi));
        
        // Largest non-clipped sphere
        float4x4 world = make_translation_matrix(eyepoint) * make_scaling_matrix(farClip * .99);
        
        render_internal(viewProj, sunDirection, world);
        
        glEnable(GL_BLEND);
        glEnable(GL_CULL_FACE);
    }
    
    virtual void recompute(float sunTheta, float turbidity, float albedo, float normalizedSunY) = 0;

};

class HosekProceduralSky : public ProceduralSky
{
    std::unique_ptr<GlShader> sky;
    HosekSkyRadianceData data;
    
    virtual void render_internal(float4x4 viewProj, float3 sunDir, float4x4 world) override
    {
        sky->bind();
        sky->uniform("ViewProjection", viewProj);
        sky->uniform("World", world);
        sky->uniform("A", data.A);
        sky->uniform("B", data.B);
        sky->uniform("C", data.C);
        sky->uniform("D", data.D);
        sky->uniform("E", data.E);
        sky->uniform("F", data.F);
        sky->uniform("G", data.G);
        sky->uniform("H", data.H);
        sky->uniform("I", data.I);
        sky->uniform("Z", data.Z);
        sky->uniform("SunDirection", sunDir);
        skyMesh.draw_elements();
        sky->unbind();
    }
    
public:
    
    HosekProceduralSky()
    {
        sky.reset(new gfx::GlShader(read_file_text("assets/shaders/sky_vert.glsl"), read_file_text("assets/shaders/sky_hosek_frag.glsl")));
        recompute(sunTheta, turbidity, albedo, normalizedSunY);
    }
    
    virtual void recompute(float sunTheta, float turbidity, float albedo, float normalizedSunY) override
    {
        data = HosekSkyRadianceData::compute(to_radians(sunTheta), turbidity, albedo, normalizedSunY);
    }

};

class PreethamProceduralSky : public ProceduralSky
{
    std::unique_ptr<GlShader> sky;
    PreethamSkyRadianceData data;
    
    virtual void render_internal(float4x4 viewProj, float3 sunDir, float4x4 world) override
    {
        sky->bind();
        sky->uniform("ViewProjection", viewProj);
        sky->uniform("World", world);
        sky->uniform("A", data.A);
        sky->uniform("B", data.B);
        sky->uniform("C", data.C);
        sky->uniform("D", data.D);
        sky->uniform("E", data.E);
        sky->uniform("Z", data.Z);
        sky->uniform("SunDirection", sunDir);
        skyMesh.draw_elements();
        sky->unbind();
    }
    
public:
    
    PreethamProceduralSky()
    {
        sky.reset(new gfx::GlShader(read_file_text("assets/shaders/sky_vert.glsl"), read_file_text("assets/shaders/sky_preetham_frag.glsl")));
        recompute(sunTheta, turbidity, albedo, normalizedSunY);
    }
    
    virtual void recompute(float sunTheta, float turbidity, float albedo, float normalizedSunY) override
    {
        data = PreethamSkyRadianceData::compute(to_radians(sunTheta), turbidity, albedo, normalizedSunY);
    }
    
};

#endif // end procedural_sky_h