#ifndef MINI_VISION_H
#define MINI_VISION_H

#include <vector>
#include <stdint.h>
#include <array>
#include <algorithm>

#include "linear_algebra.hpp"
#include "math_util.hpp"

// tofix - for prototyping
using namespace avl;

struct avl_intrin
{
    int width; // width of the image in pixels
    int height; // height of the image in pixels
    float ppx; // horizontal coordinate of the principal point of the image, as a pixel offset from the left edge
    float ppy; // vertical coordinate of the principal point of the image, as a pixel offset from the top edge
    float fx; // focal length of the image plane, as a multiple of pixel width
    float fy; // focal length of the image plane, as a multiple of pixel height
};

std::array<double, 3> rgb_to_hsv(uint8_t r, uint8_t g, uint8_t b)
{
    std::array<double, 3> hsv;
    
    double rd = (double)r / 255;
    double gd = (double)g / 255;
    double bd = (double)b / 255;
    double max = avl::max<double>(rd, gd, bd), min = avl::min<double>(rd, gd, bd);
    double h, s, v = max;
    
    double d = max - min;
    s = max == 0 ? 0 : d / max;
    
    if (max == min)
    {
        h = 0; // achromatic
    }
    else
    {
        if (max == rd)
            h = (gd - bd) / d + (gd < bd ? 6 : 0);
        else if (max == gd)
            h = (bd - rd) / d + 2;
        else if (max == bd)
            h = (rd - gd) / d + 4;
        h /= 6;
    }
    
    hsv[0] = h;
    hsv[1] = s;
    hsv[2] = v;
    
    return hsv;
}

std::array<int, 3> hsv_to_rgb(double h, double s, double v)
{
    std::array<int, 3> rgb;
    
    double r, g, b = 0.0;
    
    int i = int(h * 6);
    double f = h * 6 - i;
    double p = v * (1 - s);
    double q = v * (1 - f * s);
    double t = v * (1 - (1 - f) * s);
    
    switch (i % 6)
    {
        case 0: r = v, g = t, b = p; break;
        case 1: r = q, g = v, b = p; break;
        case 2: r = p, g = v, b = t; break;
        case 3: r = p, g = q, b = v; break;
        case 4: r = t, g = p, b = v; break;
        case 5: r = v, g = p, b = q; break;
    }
    
    rgb[0] = uint8_t(clamp((float)r * 255.0f, 0.0f, 255.0f));
    rgb[1] = uint8_t(clamp((float)g * 255.0f, 0.0f, 255.0f));
    rgb[2] = uint8_t(clamp((float)b * 255.0f, 0.0f, 255.0f));
    
    return rgb;
}

inline void depth_to_colored_histogram(std::vector<uint8_t> & img, const std::vector<uint16_t> & depthImg, const float2 size, const float2 hsvHueRange)
{
    // Cumulative histogram of depth values
    std::array<int, 256 * 256> histogram;
    std::fill(histogram.begin(), histogram.end(), 1);
    
    for (int i = 0; i < size.x * size.y; ++i)
        if (auto d = depthImg[i]) ++histogram[d];
    
    for (int i = 1; i < 256 * 256; i++)
        histogram[i] += histogram[i - 1];
    
    // Remap the cumulative histogram to the range [0-256]
    for (int i = 1; i < 256 * 256; i++)
        histogram[i] = (histogram[i] << 8) / histogram[256 * 256 - 1];
    
    auto rgb = img.data();
    for (int i = 0; i < size.x * size.y; i++)
    {
        // For valid depth values (depth > 0)
        if (uint16_t d = depthImg[i])
        {
            auto t = histogram[d]; // Use the histogram entry (in the range of [0-256]) to interpolate between nearColor and farColor
            std::array<int, 3> returnRGB = { 0, 0, 0 };
            returnRGB = hsv_to_rgb(remap<float>(t, hsvHueRange.x, hsvHueRange.y, 0, 255, true), 1.f, 1.f);
            *rgb++ = returnRGB[0];
            *rgb++ = returnRGB[1];
            *rgb++ = returnRGB[2];
        }
        else
        {
            *rgb++ = 0;
            *rgb++ = 0;
            *rgb++ = 0;
        }
    }
}

template<typename T>
inline std::vector<uint16_t> crop(const std::vector<uint16_t> & image, const int imgWidth, const int imgHeight, int x, int y, int width, int height)
{
    std::vector<uint16_t> newImage(height * width);
    for (int i = 0; i < height; i++)
    {
        int srcLineId = (i + y) * imgWidth + x;
        int desLineId = i * width;
        memcpy(newImage.data() + desLineId, image.data() + srcLineId, width * sizeof(T));
    }
    return newImage;
}

inline std::vector<std::vector<uint16_t>> subdivide_grid(const std::vector<uint16_t> & image, const int imgWidth, const int imgHeight, const int rowDivisor, const int colDivisor)
{
    std::vector<std::vector<uint16_t>> blocks(rowDivisor * colDivisor);
    
    for (int r = 0; r < blocks.size(); r++)
        blocks[r].resize((imgWidth / rowDivisor) * (imgHeight / colDivisor));
    
    // Does it fit?
    if (imgHeight % colDivisor == 0 && imgWidth % rowDivisor == 0)
    {
        int blockIdx_y = 0;
        for (int y = 0; y < imgHeight; y += imgHeight / colDivisor)
        {
            int blockIdx_x = 0;
            for (int x = 0; x < imgWidth; x += imgWidth / rowDivisor)
            {
                auto & block = blocks[blockIdx_y * rowDivisor + blockIdx_x];
                block = crop<uint16_t>(image, imgWidth, imgHeight, x, y, 8, 8);
                blockIdx_x++;
            }
            blockIdx_y++;
        }
    }
    else if (imgHeight % colDivisor != 0 || imgWidth % rowDivisor != 0)
    {
        throw std::runtime_error("Divisor doesn't fit");
    }
    
    return blocks;
}

#define KERNEL_SIZE 3
#define KERNEL_OFFSET ((KERNEL_SIZE - 1) / 2)

enum filter_type : int
{
    ERODE,
    DILATE
};

std::vector<int> box_element_3x3_identity = {0, 0, 0, 0, 1, 0, 0, 0, 0};

// 3x3 matrix -- fully square structring element
std::vector<int> box_element_3x3_square = {1, 1, 1, 1, 1, 1, 1, 1, 1};

// Todo: check odd kernel size, pass in kernel
void erode_dilate_kernel(const std::vector<uint16_t> & inputImage, std::vector<uint16_t> & outputImage, int imageWidth, int imageHeight, filter_type t)
{
    int dx, dy, wx, wy;
    int pIndex;
    
    std::vector<uint16_t> list;
    
    for (int y = 0; y < imageHeight; ++y)
    {
        for (int x = 0; x < imageWidth; ++x)
        {
            list.clear();
            
            for (dy = -KERNEL_OFFSET; dy <= KERNEL_OFFSET; ++dy)
            {
                wy = y + dy;
                
                // Clamp at Y image borders
                if (wy >= 0 && wy < imageHeight)
                {
                    for (dx = -KERNEL_OFFSET; dx <= KERNEL_OFFSET; ++dx)
                    {
                        wx = x + dx;
                        
                        // Clamp at X image borders
                        if (wx >= 0 && wx < imageWidth)
                        {
                            if (box_element_3x3_square[dy * KERNEL_SIZE + dx] == 1)
                            {
                                pIndex = (wy * imageWidth + wx);
                                uint16_t inValue = inputImage[pIndex];
                                list.push_back(inValue);
                            }
                        }
                    }
                }
            }
            
            pIndex = (y * imageWidth + x);

			if (t == filter_type::ERODE)
				outputImage[pIndex] = *std::min_element(list.begin(), list.end());

			if (t== filter_type::DILATE)
				outputImage[pIndex] = *std::max_element(list.begin(), list.end());
            
        }
    }
}

// Morphology: Opening. Erosion of an image followed by dilation.
// This function works on uint16_t depth maps. All memory needs to be preallocated.
void morphology_open(const std::vector<uint16_t> & inputImage, std::vector<uint16_t> & outputImage, int imageWidth, int imageHeight)
{
    erode_dilate_kernel(inputImage, outputImage, imageWidth, imageHeight, filter_type::ERODE);
    erode_dilate_kernel(inputImage, outputImage, imageWidth, imageHeight, filter_type::DILATE);
}

// Morphology: Closing. Dilation of an image followed by an erosion.
// This function works on uint16_t depth maps. All memory needs to be preallocated.
void morphology_close(const std::vector<uint16_t> & inputImage, std::vector<uint16_t> & outputImage, int imageWidth, int imageHeight)
{
    erode_dilate_kernel(inputImage, outputImage, imageWidth, imageHeight, filter_type::DILATE);
    erode_dilate_kernel(inputImage, outputImage, imageWidth, imageHeight, filter_type::ERODE);
}

// Morphology: Gradient. The difference image of dilation and erosion.
// This function works on uint16_t depth maps. All memory needs to be preallocated.
void morphology_gradient(const std::vector<uint16_t> & inputImage, std::vector<uint16_t> & outputImage, int imageWidth, int imageHeight)
{
    std::vector<uint16_t> dilatedImage(imageWidth * imageHeight);
    std::vector<uint16_t> erodedImage(imageWidth * imageHeight);
    
    erode_dilate_kernel(inputImage, dilatedImage, imageWidth, imageHeight, filter_type::DILATE);
    erode_dilate_kernel(inputImage, erodedImage, imageWidth, imageHeight, filter_type::ERODE);
    
    for (int i = 0; i < imageHeight; ++i)
    {
        for (int j = 0; j < imageWidth; j++)
        {
            auto idx = i * imageWidth + j;
            outputImage[idx] = dilatedImage[idx] - erodedImage[idx];
        }
    }
}

// Unoptimized NxN box filter designed to operate on float3 normal maps
void box_filter_normalmap(const std::vector<float3> & input, std::vector<float3> & output, const uint2 size, const int radius = 1)
{
    const float maxY = size.x - radius;
    const float maxX = size.y - radius;
    
    auto out = output.data();
    auto in = input.data();
    
    out += radius * size.x;
    
    for (size_t y = radius; y < maxY; ++y)
    {
        out += radius;
        for (size_t x = radius; x < maxX; ++x, ++out)
        {
            float3 avg;
            size_t index = x - radius + (y - radius) * size.x;
            const float3 * in_row = in + index;
            for (int dy = -radius; dy <= radius; ++dy, in_row += size.x)
            {
                const float3 * in_kernel = in_row;
                for (int dx = -radius; dx <= radius; ++dx, ++in_kernel)
                {
                    avg += *in_kernel;
                }
            }
            
            *out = normalize(avg);
        }
        out += radius;
    }
}

// This function derived from one found in Leo Keselman's ps1080 testing repository: https://github.com/leonidk/ps1080test
// As such, its usage is licensed under the MPL 2.0.
template <int size>
inline void generate_normalmap(const std::vector<uint16_t> & depth, std::vector<float3> & normals, const avl_intrin intrin)
{
    const auto cX = 1.0f / intrin.fx;
    const auto cY = 1.0f / intrin.fy;
    auto halfX = intrin.ppx;
    auto halfY = intrin.ppy;
    
    for (int i = size; i < intrin.height - size; i++)
    {
        for (int j = size; j < intrin.width  - size; j++)
        {
            if (!depth[i * intrin.width + j])
                continue;
            
            const auto cDepth = depth[i * intrin.width + j];
            
            const float3 pc = { cX*(j - halfX) * cDepth, cY * (i - halfY)*cDepth, (float)cDepth };
            float3 outNorm = { 0, 0, 0 };
            
            int count = 0;
            
            if (depth[i * intrin.width + j + size] && depth[(i + size) * intrin.width + j])
            {
                const auto xDepth = depth[i * intrin.width + j + size];
                const auto yDepth = depth[(i + size) * intrin.width + j];
                
                const float3 px = { cX * (j - halfX + size) * xDepth, cY * (i - halfY) * xDepth, (float)xDepth };
                const float3 py = { cX *(j - halfX) * yDepth, cY * (i - halfY + size) * yDepth, (float)yDepth };
                
                float3 v1 = { px[0] - pc[0], px[1] - pc[1], px[2] - pc[2] };
                float3 v2 = { py[0] - pc[0], py[1] - pc[1], py[2] - pc[2] };
                
                auto v3 = cross(v1, v2);
                v3 = normalize(v3);
                
                outNorm += v3;
                count++;
            }
            
            if (depth[i * intrin.width + j - size] && depth[(i + size) * intrin.width + j])
            {
                const auto xDepth = depth[i * intrin.width + j - size];
                const auto yDepth = depth[(i + size) * intrin.width + j];
                
                const float3 px = { cX * (j - halfX - size) * xDepth, cY  * (i - halfY) * xDepth, (float)xDepth };
                const float3 py = { cX *(j - halfX) * yDepth, cY * (i -  halfY + size) * yDepth, (float)yDepth };
                
                float3 v1= { pc[0] - px[0], pc[1] - px[1], pc[2] - px[2] };
                float3 v2 = { py[0] - pc[0], py[1] - pc[1], py[2] - pc[2] };
                
                auto v3 = cross(v1, v2);
                v3 = normalize(v3);
                
                outNorm += v3;
                count++;
            }
            
            if (depth[i * intrin.width + j + size] && depth[(i - size) * intrin.width + j])
            {
                const auto xDepth = depth[i * intrin.width + j + size];
                const auto yDepth = depth[(i - size) * intrin.width + j];
                
                const float3 px = { cX * (j - halfX + size) * xDepth, cY * (i - halfY) * xDepth, (float)xDepth };
                const float3 py = { cX * (j - halfX) * yDepth, cY * (i - halfY - size) * yDepth, (float)yDepth };
                
                float3 v1 = { px[0] - pc[0], px[1] - pc[1], px[2] - pc[2] };
                float3 v2 = { pc[0] - py[0], pc[1] - py[1], pc[2] - py[2] };
                
                auto v3 = cross(v1, v2);
                v3 = normalize(v3);
                
                outNorm += v3;
                count++;
            }
            
            if (depth[i * intrin.width + j - size] && depth[(i - size) * intrin.width + j])
            {
                const auto xDepth = depth[i * intrin.width + j - size];
                const auto yDepth = depth[(i - size) * intrin.width + j];
                
                const float3 px = { cX * (j - halfX - size) * xDepth, cY * (i - halfY) * xDepth, (float)xDepth };
                const float3 py = { cX * (j - halfX) * yDepth, cY * (i - halfY - size) * yDepth, (float)yDepth };
                
                float3 v1 = { pc[0] - px[0], pc[1] - px[1], pc[2] - px[2] };
                float3 v2 = { pc[0] - py[0], pc[1] - py[1], pc[2] - py[2] };
                
                auto v3 = cross(v1, v2);
                v3 = normalize(v3);
                
                outNorm += v3;
                count++;
            }
            
            if (count)
            {
                float3 v3 = { outNorm[0] / count, outNorm[1] / count, outNorm[2] / count };
                v3 = normalize(v3);
                normals[i * intrin.width + j] = v3;
            }
        }
    }
}

template<typename T>
T ComputeCenterOfMass(const std::vector<T> & points)
{
    T result;
    for (const auto & pt : points)
    {
        result += pt;
    }
    return (result / (float) points.size());
}

// Approximate volumetic subsampling, adapted from the BSD code found in PCL's VoxelGrid filter
// https://github.com/PointCloudLibrary/pcl/blob/master/filters/include/pcl/filters/voxel_grid.h
inline std::vector<float3> VoxelSubsample(const std::vector<float3> & points, float voxelSize, int minOccupants)
{
    std::vector<float3> subPoints;
    
    // Structure for storing per-voxel data
    struct Voxel { int3 coord; float3 point; int count; };
    
    const int HASH_SIZE = 2048;
    const int HASH_MASK = HASH_SIZE - 1; // Hash size must be a power of two
    
    Voxel voxelHash[HASH_SIZE];
    memset(voxelHash, 0, sizeof(voxelHash));
    
    // Store each point in corresponding voxel
    const float inverseVoxelSize = 1.0f / voxelSize;
    static const int3 hashCoeff(7171, 3079, 4231);
    
    for (const auto & pt : points)
    {
        // Obtain corresponding voxel
        float3 fcoord = vfloor(pt * inverseVoxelSize);
        int3 vcoord = int3(static_cast<int>(fcoord.x),static_cast<int>(fcoord.y),static_cast<int>(fcoord.z));
        auto hash = dot(vcoord, hashCoeff) & HASH_MASK;
        auto & voxel = voxelHash[hash];
        
        // If we collide, flush existing voxel contents
        if (voxel.count && voxel.coord != vcoord)
        {
            if(voxel.count > minOccupants)
                subPoints.push_back(voxel.point / (float)voxel.count);
            voxel.count = 0;
        }
        
        // If voxel is empty, store all properties of this point
        if (voxel.count == 0)
        {
            voxel.coord = vcoord;
            voxel.count = 1;
            voxel.point = pt;
        }
        else // Otherwise just add position contribution
        {
            voxel.point += pt;
            ++voxel.count;
        }
    }
    
    // Flush remaining voxels
    for (const auto & voxel : voxelHash )
    {
        if (voxel.count > minOccupants)
        {
            subPoints.push_back(voxel.point / (float) voxel.count);
        }
    }
    
    return subPoints;
}

// Median Filter

// Bilateral Filter

// Normals Filter

// Spatial derivative filter (edges)

// Integral Img

// NxN Median Filter

// NxN Gaussian Filter

// NxN Sobel Filter

// NxN Canny Filter

// NxN Bilateral

// 2x2 Downscale, 2x2 Upscale (NN, Bilinear, Area)

// Pyramid Up, Pyramid Down

// LK Optical Flow Pyramid

// ICP

#endif // MINI_VISION_H
