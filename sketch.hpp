#ifndef sketch_h
#define sketch_h

#include <vector>

inline std::vector<uint16_t> crop(const std::vector<uint16_t> & image, const int imgWidth, const int imgHeight, int x, int y, int width, int height)
{
    std::vector<uint16_t> newImage(height * width);
    for (int i = 0; i < height; i++)
    {
        int srcLineId = (i + y) * imgWidth + x;
        int desLineId = i * width;
        memcpy(newImage.data() + desLineId, image.data() + srcLineId, width * 2);
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
                block = crop(image, imgWidth, imgHeight, x, y, 8, 8);
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

#endif // sketch_h
