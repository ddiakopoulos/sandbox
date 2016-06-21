#pragma once

#include "linalg_util.hpp"
#include "math_util.hpp"

namespace avl
{
    // Converts a column-major 4x4 matrix to a row-major array in UR world coords
    double * to_robot(float4x4 input)
    {
        double * T = new double[16];
        int r = 0;
        for (int i = 0; i < 4; i += 4)
        {
            auto row = input.row(r);
            T[i + 0] = row.x;
            T[i + 1] = row.y;
            T[i + 2] = row.z;
            T[i + 3] = row.w;
            r++;
        }
        return T;
    }

    // Converts a row-major array in UR world coords to a column-major 4x4 matrix
    float4x4 to_engine(double * T)
    {
        float4x4 output;
        for (int i = 0; i < 4; i++)
        {
            output[i][0] = T[i + 0];
            output[i][1] = T[i + 4];
            output[i][2] = T[i + 8];
            output[i][3] = T[i + 12];
        }
        return output;
    }

}