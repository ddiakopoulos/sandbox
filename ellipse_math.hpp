// See COPYING file for attribution information

#pragma once
#ifndef ellipse_math_h
#define ellipse_math_h

#include "linear_algebra.hpp"
#include <cmath>

namespace avl
{
    // The bisection algorithm to find the unique root of F(t).
    inline float point_on_ellipse_bisector(int numComponents, const float2 &extents, const float2 &y, float2& x)
    {
        float2 z;
        float sumZSqr = 0;
        int i;
        for (i = 0; i < numComponents; ++i) 
        {
            z[i] = y[i] / extents[i];
            sumZSqr += z[i] * z[i];
        }

        if (sumZSqr == 1) 
        {
            // The point is on the hyperellipsoid.
            for (i = 0; i < numComponents; ++i)
                x[i] = y[i];

            return 0;
        }

        float emin = extents[numComponents - 1];
        float2 pSqr, numerator;
        for (i = 0; i < numComponents; ++i) 
        {
            float p = extents[i] / emin;
            pSqr[i] = p * p;
            numerator[i] = pSqr[i] * z[i];
        }

        // The maximum number of bisections required for Real before the interval
        // endpoints are equal (as floating-point numbers).
        int const jmax = std::numeric_limits<float>::digits - std::numeric_limits<float>::min_exponent;

        float s = 0, smin = z[numComponents - 1] - 1, smax;
        if (sumZSqr < 1)
            // The point is strictly inside the hyperellipsoid.
            smax = 0;
        else
            // The point is strictly outside the hyperellipsoid.
            //smax = LengthRobust(numerator) - (Real)1;
            smax = length(numerator) - 1;

        for (int j = 0; j < jmax; ++j) 
        {
            s = (smin + smax) * 0.5f;
            if (s == smin || s == smax)
                break;

            float g = -1;
            for (i = 0; i < numComponents; ++i) 
            {
                float ratio = numerator[i] / (s + pSqr[i]);
                g += ratio * ratio;
            }

            if (g > 0)
                smin = s;
            else if (g < 0)
                smax = s;
            else
                break;
        }

        float sqrDistance = 0;
        for (i = 0; i < numComponents; ++i) 
        {
            x[i] = pSqr[i] * y[i] / (s + pSqr[i]);
            float diff = x[i] - y[i];
            sqrDistance += diff*diff;
        }
        return sqrDistance;
    }

    // The hyperellipsoid is sum_{d=0}^{N-1} (x[d]/e[d])^2 = 1 with the e[d]
    // positive and nonincreasing:  e[d+1] >= e[d] for all d.  The query
    // point is (y[0],...,y[N-1]) with y[d] >= 0 for all d.  The function
    // returns the squared distance from the query point to the
    // hyperellipsoid.  It also computes the hyperellipsoid point
    // (x[0],...,x[N-1]) that is closest to (y[0],...,y[N-1]), where
    // x[d] >= 0 for all d.
    inline float point_on_ellipse_sqr_distance_special(const float2 &extents, const float2 &y, float2 &x)
    {
        float sqrDistance = 0;

        float2 ePos, yPos, xPos;
        int numPos = 0;
        for (int i = 0; i < 2; ++i) 
        {
            if (y[i] > 0) 
            {
                ePos[numPos] = extents[i];
                yPos[numPos] = y[i];
                ++numPos;
            }
            else
                x[i] = 0;
        }

        if (y[2 - 1] > 0)
            sqrDistance = point_on_ellipse_bisector(numPos, ePos, yPos, xPos);
        else 
        {
            // y[N-1] = 0
            float numer[1], denom[1];
            float eNm1Sqr = extents[2 - 1] * extents[2 - 1];
            for (int i = 0; i < numPos; ++i)
            
            {
                numer[i] = ePos[i] * yPos[i];
                denom[i] = ePos[i] * ePos[i] - eNm1Sqr;
            }

            bool inSubHyperbox = true;
            for (int i = 0; i < numPos; ++i) 
            {
                if (numer[i] >= denom[i]) 
                {
                    inSubHyperbox = false;
                    break;
                }
            }

            bool inSubHyperellipsoid = false;
            if (inSubHyperbox) 
            {
                // yPos[] is inside the axis-aligned bounding box of the
                // subhyperellipsoid.  This intermediate test is designed to guard
                // against the division by zero when ePos[i] == e[N-1] for some i.
                float xde[1];
                float discr = 1;
                for (int i = 0; i < numPos; ++i)
                
                {
                    xde[i] = numer[i] / denom[i];
                    discr -= xde[i] * xde[i];
                }
                if (discr > 0) 
                {
                    // yPos[] is inside the subhyperellipsoid.  The closest
                    // hyperellipsoid point has x[N-1] > 0.
                    sqrDistance = 0;
                    for (int i = 0; i < numPos; ++i)
                    
                    {
                        xPos[i] = ePos[i] * xde[i];
                        float diff = xPos[i] - yPos[i];
                        sqrDistance += diff*diff;
                    }
                    x[2 - 1] = extents[2 - 1] * sqrt(discr);
                    sqrDistance += x[2 - 1] * x[2 - 1];
                    inSubHyperellipsoid = true;
                }
            }

            if (!inSubHyperellipsoid)
            {
                // yPos[] is outside the subhyperellipsoid.  The closest
                // hyperellipsoid point has x[N-1] == 0 and is on the
                // domain-boundary hyperellipsoid.
                x[2 - 1] = 0;
                sqrDistance = point_on_ellipse_bisector(numPos, ePos, yPos, xPos);
            }
        }

        // Fill in those x[] values that were not zeroed out initially.
        for (int i = 0, numPos = 0; i < 2; ++i) 
        {
            if (y[i] > 0) 
            {
                x[i] = xPos[numPos];
                ++numPos;
            }
        }

        return sqrDistance;
    }

    // The hyperellipsoid is sum_{d=0}^{N-1} (x[d]/e[d])^2 = 1 with no
    // constraints on the orderind of the e[d].  The query point is
    // (y[0],...,y[N-1]) with no constraints on the signs of the components.
    // The function returns the squared distance from the query point to the
    // hyperellipsoid.   It also computes the hyperellipsoid point
    // (x[0],...,x[N-1]) that is closest to (y[0],...,y[N-1]).
    inline float point_on_ellipse_sqr_distance(const float2 &extents, const float2 &y, float2 &x)
    {
        // Determine negations for y to the first octant.
        bool negate[2];
        for (int i = 0; i < 2; ++i)
            negate[i] = y[i] < 0;

        // Determine the axis order for decreasing extents.
        std::pair<float, int> permute[2];
        for (int i = 0; i < 2; ++i) 
        {
            permute[i].first = -extents[i];
            permute[i].second = i;
        }
        std::sort(&permute[0], &permute[2]);

        int invPermute[2];
        for (int i = 0; i < 2; ++i)
            invPermute[permute[i].second] = i;

        float2 locE, locY;
        int j;
        for (int i = 0; i < 2; ++i) 
        {
            j = permute[i].second;
            locE[i] = extents[j];
            locY[i] = std::abs(y[j]);
        }

        float2 locX;
        float sqrDistance = point_on_ellipse_sqr_distance_special(locE, locY, locX);

        // Restore the axis order and reflections.
        for (int i = 0; i < 2; ++i) 
        {
            j = invPermute[i];
            if (negate[i])
                locX[j] = -locX[j];
            x[i] = locX[j];
        }

        return sqrDistance;
    }

    // Compute the distance from a point to a hyperellipsoid.  In 2D, this is a
    // point-ellipse distance query.  In 3D, this is a point-ellipsoid distance
    // query.  The following document describes the algorithm.
    //   http://www.geometrictools.com/Documentation/DistancePointEllipseEllipsoid.pdf
    // The hyperellipsoid can have arbitrary center and orientation; that is, it
    // does not have to be axis-aligned with center at the origin.
    inline float2 get_closest_point_on_ellipse(const float2 & center, const float2 & axisA, const float2 & axisB, const float2 & testPoint)
    {
        // Compute the coordinates of Y in the hyperellipsoid coordinate system.
        float lengthA = length(axisA);
        float lengthB = length(axisB);

        float2 unitA = axisA / lengthA;
        float2 unitB = axisB / lengthB;
        float2 diff = testPoint - center;
        float2 y(dot(diff, unitA), dot(diff, unitB));

        // Compute the closest hyperellipsoid point in the axis-aligned coordinate system.
        float2 x;
        float2 extents(lengthA, lengthB);
        point_on_ellipse_sqr_distance(extents, y, x);

        // Convert back to the original coordinate system.
        float2 result = center;
        result += x[0] * unitA;
        result += x[1] * unitB;

        return result;
    }

}

#endif // ellipse_math_h