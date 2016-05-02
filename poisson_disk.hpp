// See COPYING file for attribution information, Based on https://github.com/simongeilfus/PoissonDiskDistribution

#pragma once

#ifndef poisson_disk_sampling_h
#define poisson_disk_sampling_h

#include "geometric.hpp"
#include <functional>
#include <random>

namespace 
{
    using namespace linalg;
    using namespace linalg::aliases;
    
    class RandomGenerator
    {
        std::random_device rd;
        std::mt19937 gen;
        std::uniform_real_distribution<float> distribution;
    public:
        RandomGenerator() : rd(), gen(rd()), distribution(0.0f, 1.0f) { }
        float random_float() { return static_cast<float>(distribution(gen)); }
        int random_int(int max) { std::uniform_int_distribution<int> dInt(0, max); return dInt(gen); }
    };

    class Grid 
    {
        std::vector<std::vector<float2>> grid;
        int2 numCells, offset;
        Bounds2D bounds;
        uint32_t kValue, cellSize;

    public:

        Grid(const Bounds2D & bounds, uint32_t k) { resize(bounds, k); }

        void add(const float2 & position)
        {
            int x = uint32_t(position.x + offset.x) >> kValue;
            int y = uint32_t(position.y + offset.y) >> kValue;
            int j = x + numCells.x * y;
            if(j < grid.size()) grid[j].push_back(position);
        }

        bool has_neighbors(const float2 & p, float radius)
        {
            float sqRadius = radius * radius;
            int2 radiusVec = int2(radius);
            int2 min = linalg::max(linalg::min(int2(p) - radiusVec, int2(bounds.max()) - int2(1)), int2(bounds.min()));
            int2 max = linalg::max(linalg::min(int2(p) + radiusVec, int2(bounds.max()) - int2(1)), int2(bounds.min()));
            
            int2 minCell = int2((min.x + offset.x) >> kValue, (min.y + offset.y) >> kValue);
            int2 maxCell = linalg::min(1 + int2((max.x + offset.x) >> kValue, (max.y + offset.y) >> kValue), numCells);
            
            for (size_t y = minCell.y; y < maxCell.y; y++)
            {
                for (size_t x = minCell.x; x < maxCell.x; x++) 
                {
                    for (auto cell : grid[x + numCells.x * y])
                    {
                        if (length2(p - cell) < sqRadius)
                            return true;
                    }
                }
            }
            return false;
        }

        void resize(const Bounds2D & bounds, uint32_t k)
        {
            this->bounds = bounds;
            resize(k);
        }

        void resize(uint32_t k)
        {
            kValue = k;
            cellSize = 1 << k;
            offset = int2(abs(bounds.min()));
            numCells = int2(ceil(float2(bounds.size()) / (float) cellSize));
            grid.clear();
            grid.resize(numCells.x * numCells.y);
        }
    };
    
    struct PoissonDiskGenerator
    {
        std::function<float(const float2 &)> distFunction;
        std::function<bool(const float2 &)> boundsFunction;
        
        std::vector<float2> build(const Bounds2D & bounds, const std::vector<float2> & initialSet, int k, float separation = 1.0)
        {
            std::vector<float2> processingList;
            std::vector<float2> outputList;
            Grid grid(bounds, 3);
            RandomGenerator r;
            
            // add the initial points
            for(auto p : initialSet)
            {
                processingList.push_back(p);
                outputList.push_back(p);
                grid.add(p);
            }
            
            // if there's no initial points add the center point
            if(!processingList.size())
            {
                processingList.push_back(bounds.center());
                outputList.push_back(bounds.center());
                grid.add(bounds.center());
            }
            
            // while there's points in the processing list
            while(processingList.size())
            {
                // pick a random point in the processing list
                int randPoint = r.random_int(int(processingList.size()) - 1);
                float2 center = processingList[randPoint];
                
                // remove it
                processingList.erase(processingList.begin() + randPoint);
                
                if (distFunction)
                    separation = distFunction(center);
                
                // spawn k points in an anulus around that point
                // the higher k is, the higher the packing will be and slower the algorithm
                for(int i = 0; i < k; i++)
                {
                    float randRadius = separation * (1.0f + r.random_float());
                    float randAngle = r.random_float() * ANVIL_PI * 2.0f;
                    float2 newPoint = center + float2(cos(randAngle), sin(randAngle)) * randRadius;
                    
                    // check if the new random point is in the window bounds
                    // and if it has no neighbors that are too close to them
                    if(bounds.contains(newPoint) && !grid.has_neighbors(newPoint, separation))
                    {
                        if (boundsFunction && boundsFunction(newPoint)) continue;
                        
                        // if the point has no close neighbors add it to the processing list, output list and grid
                        processingList.push_back(newPoint);
                        outputList.push_back(newPoint);
                        grid.add(newPoint);
                    }
                }
            }
            
            return outputList;
        }
    };

}

// Returns a set of poisson disk samples inside a rectangular area, with a minimum separation and with
// a packing determined by how high k is. The higher k is the higher the algorithm will be slow.
// If no initialSet of points is provided the area center will be used as the initial point.
inline std::vector<float2> make_poisson_disk_distribution(const Bounds2D & bounds, const std::vector<float2> & initialSet, int k, float separation = 1.0)
{
    ::PoissonDiskGenerator gen;
    return gen.build(bounds, initialSet, k, separation);
}

#endif