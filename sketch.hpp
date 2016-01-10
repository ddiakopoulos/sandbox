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

#endif // sketch_h
