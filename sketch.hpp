#ifndef sketch_h
#define sketch_h

#include <vector>
#include "linear_algebra.hpp"
#include "math_util.hpp"
#include "geometric.hpp"

class VoxelArray
{
    math::int3 size;
    std::vector<uint32_t> voxels;
public:
    VoxelArray() {}
    VoxelArray(const math::int3 & size) : size(size), voxels(size.x * size.y * size.z) {}
    const math::int3 & get_size() const { return size; }
    uint32_t operator[](const math::int3 & coords) const { return voxels[coords.z * size.x * size.y + coords.y * size.x + coords.x]; }
    uint32_t & operator[](const math::int3 & coords) { return voxels[coords.z * size.x * size.y + coords.y * size.x + coords.x]; }
};

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

void erode_dilate_kernel(const std::vector<uint16_t> & inputImage, std::vector<uint16_t> & outputImage, int imageWidth, int imageHeight, filter_type t)
{
    int dx, dy, wx, wy;
    int pIndex;
    uint16_t compare;
    uint8_t foundMatch;
    
    if (t == filter_type::ERODE)
        compare = std::numeric_limits<uint16_t>::min(); // Erode: Neighbor pixels in background
    else
        compare = std::numeric_limits<uint16_t>::max(); // Dilate: Look for neighbor pixels in foreground
    
    for (int y = 0; y < imageHeight; ++y)
    {
        for (int x = 0; x < imageWidth; ++x)
        {
            foundMatch = 0;
            for (dy = -KERNEL_OFFSET; dy <= KERNEL_OFFSET; ++dy)
            {
                wy = y + dy;
                if (wy >= 0 && wy < imageHeight)
                {
                    for (dx = -KERNEL_OFFSET; dx <= KERNEL_OFFSET; ++dx)
                    {
                        wx = x + dx;
                        if (wx >= 0 && wx < imageWidth)
                        {
                            pIndex = (wy * imageWidth + wx);
                            uint16_t inValue = inputImage[pIndex];
                            if (inValue == compare)
                            {
                                foundMatch = 1;
                                break;
                            }
                        }
                    }
                }
                if (foundMatch)
                    break;
            }
            
            pIndex = (y * imageWidth + x);
            
            // Default: set background color
            outputImage[pIndex] = compare;
            
            // Output pixel max
            if ( (t == filter_type::ERODE && !foundMatch) || (t == filter_type::DILATE && foundMatch))
            {
                outputImage[pIndex] = compare;
            }
            
        }
    }
}

#endif // sketch_h
