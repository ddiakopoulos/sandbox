#ifndef MINI_VISION_H
#define MINI_VISION_H

#include <vector>
#include <stdint.h>

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

// Open Op

// Close Op

// Color space conversions

// Equalize Histogram

// ICP

// Integral Img

// Generate normals

// NxN Median Filter

// NxN Gaussian Filter

// NxN Sobel Filter

// NxN Canny Filter

// NxN Box Filter

// NxN Bilateral

// 2x2 Downscale, 2x2 Upscale (NN, Bilinear, Area)

// Pyramid Up, Pyramid Down

// LK Optical Flow Pyramid

#endif // MINI_VISION_H
