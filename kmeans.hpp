#ifndef kmeans_clustering_hpp
#define kmeans_clustering_hpp

#include "util.hpp"
#include "linalg_util.hpp"
#include <assert.h>

using namespace avl;

uint32_t kmeans_cluster3d(const std::vector<float3> & input,                   // an array of input 3d data points.
                             uint32_t inputSize,                // the number of input data points.
                             uint32_t clumpCount,               // the number of clumps you wish to produce
                            std::vector<float3> & clusters,              // The output array of clumps 3d vectors, should be at least 'clumpCount' in size.
                            std::vector<uint32_t> & outputIndices,        // A set of indices which remaps the input vertices to clumps; should be at least 'inputSize'
                             float errorThreshold,              // The error threshold to converge towards before giving up.
                             float collapseDistance)            // distance so small it is not worth bothering to create a new clump.
{

    uint32_t convergeCount = 64; // Maximum number of iterations attempting to converge to a solution.

    std::vector<uint32_t> counts(clumpCount);

    float error = 0.f;

    if (inputSize <= clumpCount) // if the number of input points is less than our clumping size, just return the input points.
    {
        clumpCount = inputSize;
        for (uint32_t i=0; i<inputSize; i++)
        {
            outputIndices[i] = i;
            clusters[i] = input[i];
            counts[i] = 1;
        }
    }
    else
    {
        std::vector<float3> centroids(clumpCount);

        // Take a sampling of the input points as initial centroid estimates.
        for (uint32_t i=0; i<clumpCount; i++)
        {
            uint32_t index = (i*inputSize)/clumpCount;
            assert(index < inputSize);
            clusters[i] = input[index];
        }

        // Here is the main convergence loop
        float old_error = std::numeric_limits<float>::max();   // old and initial error estimates
        error = old_error;

        do
        {
            old_error = error;  // preserve the old error

            // reset the counts and centroids to current cluster location
            for (uint32_t i=0; i<clumpCount; i++)
            {
                counts[i] = 0;
                centroids[i] = float3(0, 0, 0);
            }
            error = 0;

            // For each input data point, figure out which cluster it is closest too and add it to that cluster.
            for (uint32_t i=0; i<inputSize; i++)
            {
                float minDistance  = std::numeric_limits<float>::max();
                // find the nearest clump to this point.
                for (uint32_t j=0; j<clumpCount; j++)
                {
                    float distance = linalg::distance2(input[i], clusters[j]);
                    if (distance < minDistance)
                    {
                        minDistance = distance;
                        outputIndices[i] = j; // save which clump this point indexes
                    }
                }
                uint32_t index = outputIndices[i]; // which clump was nearest to this point.
                centroids[index]+=input[i];
                counts[index]++;    // increment the counter indicating how many points are in this clump.
                error+=minDistance; // save the error accumulation
            }

            // Now, for each clump, compute the mean and store the result.
            for (uint32_t i=0; i < clumpCount; i++)
            {
                if (counts[i]) // if this clump got any points added to it...
                {
                    float3 recip = float3(1.0f / counts[i]);    // compute the average (center of those points)
                    centroids[i] *= recip;    // compute the average center of the points in this clump.
                    clusters[i] = centroids[i]; // store it as the new cluster.
                }
            }

            // decrement the convergence counter and bail if it is taking too long to converge to a solution.
            convergeCount--;
            if (convergeCount == 0)
            {
                break;
            }

            if (error < errorThreshold) // early exit if our first guess is already good enough (if all input points are the same)
                break;

        } while (std::fabs(error - old_error) > errorThreshold); // keep going until the error is reduced by this threshold amount.

    }

    // ok..now we prune the clumps if necessary.
    // The rules are; first, if a clump has no 'counts' then we prune it as it's unused.
    // The second, is if the centroid of this clump is essentially  the same (based on the distance tolerance)
    // as an existing clump, then it is pruned and all indices which used to point to it, now point to the one
    // it is closest too.
    uint32_t outCount = 0; // number of clumps output after pruning performed.
    float d2 = collapseDistance * collapseDistance; // squared collapse distance.
    for (uint32_t i=0; i<clumpCount; i++)
    {
        if (counts[i] == 0) // if no points ended up in this clump, eliminate it.
            continue;
        // see if this clump is too close to any already accepted clump.
        bool add = true;
        uint32_t remapIndex = outCount; // by default this clump will be remapped to its current index.
        for (uint32_t j=0; j<outCount; j++)
        {
            float distance = linalg::distance2(clusters[i], clusters[j]);
            if (distance < d2)
            {
                remapIndex = j;
                add = false; // we do not add this clump
                break;
            }
        }
        // If we have fewer output clumps than input clumps so far, then we need to remap the old indices to the new ones.
        if (outCount != i || !add) // we need to remap indices!  everything that was index 'i' now needs to be remapped to 'outCount'
        {
            for (uint32_t j=0; j<inputSize; j++)
            {
                if (outputIndices[j] == i)
                {
                    outputIndices[j] = remapIndex;
                }
            }
        }
        if (add)
        {
            clusters[outCount] = clusters[i];
            outCount++;
        }
    }

    clumpCount = outCount;
    return clumpCount;

};

/*
uint32_t kmeans_cluster3d(const float *input,          // an array of input 3d data points.
                             uint32_t inputSize,             // the number of input data points.
                             uint32_t clumpCount,            // the number of clumps you wish to produce
                             float    *outputClusters,    // The output array of clumps 3d vectors, should be at least 'clumpCount' in size.
                             uint32_t    *outputIndices,     // A set of indices which remaps the input vertices to clumps; should be at least 'inputSize'
                             float errorThreshold,        // The error threshold to converge towards before giving up.
                             float collapseDistance)      // distance so small it is not worth bothering to create a new clump.
{
    const Vec3d< float > *_input = (const Vec3d<float> *)input;
    Vec3d<float> *_output = (Vec3d<float> *)outputClusters;
    return kmeans_cluster< Vec3d<float>, float >(_input,inputSize,clumpCount,_output,outputIndices,errorThreshold,collapseDistance);
}

*/

#endif