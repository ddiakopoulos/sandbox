#pragma once

#ifndef constant_spline_h
#define constant_spline_h

#include "math_util.hpp"

struct SplinePoint
{
    float3 point;
    float distance = 0.0f;
    float ac = 0.0f;
    SplinePoint(){};
    SplinePoint(float3 p, float d, float ac) : point(p), distance(d), ac(ac) {}
};

class ConstantSpline
{
    float3 p0;
    float3 p1;
    float3 p2;
    float3 p3;

    float3 tmp;
    float3 res;

    std::vector<SplinePoint> points;
    std::vector<SplinePoint> lPoints;

    float inc = 0.01f;
    float d = 0.0f;
    
    // This object creates a B-Spline using 4 points, and a number steps or a fixed step distance can be specified to create a set of points that cover the curve at constant rate.
    ConstantSpline() { };

    void calculate()
    {
        d = 0.0f;
        points.clear();
        
        for (float j = 0; j <= 1.0f; j += inc)
        {
            float i = (1 - j);
            float ii = i * i;
            float iii = ii * i;
            
            float jj = j * j;
            float jjj = jj * j;
            
            res = float3(0, 0, 0);
            
            tmp = p0;
            tmp *= iii;
            res += tmp;
            
            tmp = p1;
            tmp *= 3 * j * ii;
            res += tmp;
            
            tmp = p2;
            tmp *= 3 * jj * i;
            res += tmp;
            
            tmp = p3;
            tmp *= jjj;
            res += tmp;
            
            points.emplace_back(res, 0.0f, 0.0f);
        }
        
        points.emplace_back(p3, 0.0f, 0.0f);
    }

    void calculate_distances()
    {
        d = 0.0f;
        
        SplinePoint to;
        SplinePoint from;
        float td = 0.0f;
        
        for (size_t j = 0; j < points.size() - 1; j++)
        {
            points[j].distance = td;
            points[j].ac = d;
            
            from = points[j];
            to = points[j + 1];
            td = distance(to.point, from.point);
            
            d += td;
        }
        
        points[points.size() - 1].distance = 0;
        points[points.size() - 1].ac = d;
    }


    float split_segment(const float distancePerStep, SplinePoint & a, const SplinePoint & b, std::vector<SplinePoint> & l)
    {
        SplinePoint t = b;
        float d = 0.0f;
        
        t.point -= a.point;
        
        auto rd = length(t.point);
        t.point = normalize(t.point);
        t.point *= distancePerStep;
        
        auto s = std::floor(rd / distancePerStep);
        
        for (size_t i = 0; i < s; ++i)
        {
            a.point += t.point;
            l.push_back(a);
            d += distancePerStep;
        }
        return d;
    }


    void reticulate(uint32_t steps)
    {
        float distancePerStep = d / float(steps);
        
        lPoints.clear();
        
        float localD = 0.f;
        
        // First point
        SplinePoint current = points[0];
        lPoints.push_back(current);
        
        // Reticulate
        for (size_t i = 0; i < points.size(); ++i)
        {
            if (points[i].ac - localD > distancePerStep)
            {
                localD += split_segment(distancePerStep, current, points[i], lPoints);
            }
        }
        
        // Last point
        lPoints.push_back(points[points.size() - 1]);
    }
    
};

#endif // constant_spline_h
