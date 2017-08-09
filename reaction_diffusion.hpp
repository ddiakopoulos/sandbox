#ifndef reaction_diffusion_h
#define reaction_diffusion_h

#include "util.hpp"

#if defined(ANVIL_PLATFORM_WINDOWS)
#pragma warning(push)
#pragma warning(disable : 4244)
#endif

#include "math_util.hpp"

// http://mrob.com/pub/comp/xmorphia/
// http://n-e-r-v-o-u-s.com/education/simulation/ethworkshop.php
// "The reaction-diffusion system described here involves two generic chemical species U and V,
// whose concentration at a given point in space is referred to by variables u and v. As the term implies,
// they react with each other, and they diffuse through the medium. Therefore the concentration
// of U and V at any given location changes with time and can differ from that at other locations."

namespace avl
{
    
class GrayScottSimulator
{
    std::vector<double> u, v;
    std::vector<double> uu, vv;
    float2 size;
    double f, k;
    double dU, dV;
    bool tile = false;
    
public:
    
    GrayScottSimulator(float2 size, bool tile) : size(size), tile(tile)
    {
        const auto s = size.x * size.y;
        
        u.resize(s);
        v.resize(s);
        uu.resize(s);
        vv.resize(s);
        
        reset();
        
        set_coefficients(0.025, 0.077, 0.16, 0.08);
    }
    
    std::vector<double> & output_v() { return v; }
    std::vector<double> & output_u() { return u; }
    
    void reset()
    {
        for (uint32_t i = 0; i < uu.size(); i++)
        {
            uu[i] = 1.0;
            vv[i] = 0.0;
        }
    }
    
    double u_parameter_at(uint32_t x, uint32_t y)
    {
        if (y < size.y && x < size.x)
            return u[y * size.x + x];
        return 0;
    }
    
    double v_parameter_at(uint32_t x, uint32_t y)
    {
        if (y < size.y && x < size.x)
            return v[y * size.x + x];
        return 0;
    }
    
    void seed_image(const std::vector<uint8_t> & pixels, uint32_t imgWidth, uint32_t imgHeight)
    {
        
        uint32_t xo = clamp<double>((size.x - imgWidth) / 2, 0, size.x - 1);
        uint32_t yo = clamp<double>((size.y - imgHeight) / 2, 0, size.y - 1);
        imgWidth = min<uint32_t>(imgWidth, size.x);
        imgHeight = min<uint32_t>(imgHeight, size.y);
        
        for (uint32_t y = 0; y < imgHeight; y++)
        {
            uint32_t i = y * imgWidth;
            for (uint32_t x = 0; x < imgWidth; x++)
            {
                if (0 < (pixels[i + x] & 0xff))
                {
                    uint32_t idx = (yo + y) * size.x + xo + x;
                    uu[idx] = 0.5f;
                    vv[idx] = 0.25f;
                }
            }
        }
    }
    
    void set_coefficients(double f, double k, double dU, double dV)
    {
        this->f = f;
        this->k = k;
        this->dU = dU;
        this->dV = dV;
    }
    
    void trigger_region(uint32_t x, uint32_t y, uint32_t w, uint32_t h)
    {
        uint32_t miX = clamp<uint32_t>(x - w / 2, 0, size.x);
        uint32_t maX = clamp<uint32_t>(x + w / 2, 0, size.x);
        uint32_t miY = clamp<uint32_t>(y - h / 2, 0, size.y);
        uint32_t maY = clamp<uint32_t>(y + h / 2, 0, size.y);
        
        for (uint32_t yy = miY; yy < maY; yy++)
        {
            for (uint32_t xx = miX; xx < maX; xx++)
            {
                uint32_t idx = yy * size.x + xx;
                uu[idx] = 0.5f;
                vv[idx] = 0.25f;
            }
        }
    }
    
    void update(double t)
    {
        t = clamp<double>(t, 0, 1.0);
        uint32_t w1 = size.x - 1;
        uint32_t h1 = size.y - 1;
        
        // Solve the PDE (standard laplacian)
        for (uint32_t y = 1; y < h1; y++)
        {
            for (uint32_t x = 1; x < w1; x++)
            {
                uint32_t idx = y * size.x + x;
                
                uint32_t top = idx - size.x;
                uint32_t bottom = idx + size.x;
                uint32_t left = idx - 1;
                uint32_t right = idx + 1;
                
                double currF = f;
                double currK = k;;
                double currU = uu[idx];
                double currV = vv[idx];
                double d2 = currU * currV * currV;
                
                // Sum the second derivative in each of U and V
                u[idx] = std::max(0.0, currU + t * ((dU * ((uu[right] + uu[left] + uu[bottom] + uu[top]) - 4 * currU) - d2) + currF * (1.0 - currU)));
                v[idx] = std::max(0.0, currV + t * ((dV * ((vv[right] + vv[left] + vv[bottom] + vv[top]) - 4 * currV) + d2) - currK * currV));
            }
        }
        
        if (tile)
        {
            uint32_t w2 = w1 - 1;
            uint32_t idxH1 = h1 * size.x;
            uint32_t idxH2 = (h1 - 1) * size.x;
            
            for (uint32_t x = 0; x < size.x; x++)
            {
                uint32_t left = (x == 0 ? w1 : x - 1);
                uint32_t right = (x == w1 ? 0 : x + 1);
                uint32_t idx = idxH1 + x;
                
                double cu = uu[x];
                double cv = vv[x];
                double cui = uu[idx];
                double cvi = vv[idx];
                double d = cu * cv * cv;
                
                u[x] = std::max(0.0, cu + t * ((dU * ((uu[right] + uu[left] + uu[size.x + x] + cui) - 4 * cu) - d) + f * (1.0 - cu)));
                v[x] = std::max(0.0, cv + t * ((dV * ((vv[right] + vv[left] + vv[size.x + x] + cvi) - 4 * cv) + d) - k * cv));
                
                d = cui * cvi * cvi;
                
                u[idx] = std::max(0.0, cui + t * ((dU * ((uu[idxH1 + right] + uu[idxH1 + left] + cu + uu[idxH2 + x]) - 4 * cui) - d) + f * (1.0 - cui)));
                v[idx] = std::max(0.0, cvi + t * ((dU * ((vv[idxH1 + right] + vv[idxH1 + left] + cv + vv[idxH2 + x]) - 4 * cvi) + d) - k * cvi));
            }
            
            for (uint32_t y = 0; y < size.y; y++)
            {
                uint32_t idx = y * size.x;
                uint32_t idxW1 = idx + w1;
                uint32_t idxW2 = idx + w2;
                double cu = uu[idx];
                double cv = vv[idx];
                double cui = uu[idxW1];
                double cvi = vv[idxW1];
                double d = cu * cv * cv;
                
                uint32_t up = (y == 0 ? h1 : y - 1) * size.x;
                uint32_t down = (y == h1 ? 0 : y + 1) * size.x;
                
                u[idx] = std::max(0.0, cu + t * ((dU * ((uu[idx + 1] + cui + uu[down] + uu[up]) - 4 * cu) - d) + f * (1.0 - cu)));
                v[idx] = std::max(0.0, cv + t * ((dV * ((vv[idx + 1] + cvi + vv[down] + vv[up]) - 4 * cv) + d) - k * cv));
                
                d = cui * cvi * cvi;
                
                u[idxW1] = std::max(0.0, cui + t * ((dU * ((cu + uu[idxW2] + uu[down + w1] + uu[up + w1]) - 4 * cui) - d) + f * (1.0 - cui)));
                v[idxW1] = std::max(0.0, cvi + t  * ((dV * ((cv + vv[idxW2] + vv[down + w1] + vv[up + w1]) - 4 * cvi) + d) - k * cvi));
            }
        }
        
        uu = u;
        vv = v;
    }
};
    
}

#pragma warning(pop) 

#endif // reaction_diffusion_h
