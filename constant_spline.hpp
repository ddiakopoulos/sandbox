// See COPYING file for attribution information

#pragma once

#ifndef constant_spline_h
#define constant_spline_h

#include "math_util.hpp"

namespace avl
{
    
struct SplinePoint
{
    float3 point;
    float distance = 0.0f;
    float ac = 0.0f;
    SplinePoint(){};
    SplinePoint(float3 p, float d, float ac) : point(p), distance(d), ac(ac) {}
};

// This object creates a B-Spline using 4 points, and a number steps
// or a fixed step distance can be specified to create a set of points that cover the curve at constant rate.
class ConstantSpline
{
    std::vector<SplinePoint> points;
    std::vector<SplinePoint> lPoints;
    
public:
    
    float3 p0, p1, p2, p3;
    
    float d = 0.0f;
    
    ConstantSpline() { };

    void calculate(float increment = 0.01f)
    {
        d = 0.0f;
        points.clear();
        
        float3 tmp, result;
        for (float j = 0; j <= 1.0f; j += increment)
        {
            float i = (1 - j);
            float ii = i * i;
            float iii = ii * i;
            
            float jj = j * j;
            float jjj = jj * j;
            
            result = float3(0, 0, 0);
            
            tmp = p0;
            tmp *= iii;
            result += tmp;
            
            tmp = p1;
            tmp *= 3 * j * ii;
            result += tmp;
            
            tmp = p2;
            tmp *= 3 * jj * i;
            result += tmp;
            
            tmp = p3;
            tmp *= jjj;
            result += tmp;
            
            points.emplace_back(result, 0.0f, 0.0f);
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
        float distance = 0.0f;
        
        t.point -= a.point;
        
        auto rd = length(t.point);
        t.point = safe_normalize(t.point);
        t.point *= distancePerStep;
        
        auto s = std::floor(rd / distancePerStep);
        
        for (size_t i = 0; i < s; ++i)
        {
            a.point += t.point;
            l.push_back(a);
            distance += distancePerStep;
        }
        return distance;
    }

    // In Will Wright's own words:
    //  "Construct network based functions that are defined by divisible intervals
    //   while approximating said network and composing it of pieces of simple functions defined on
    //   subintervals and joined at their endpoints with a suitable degree of smoothness."
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
    
    std::vector<float3> get_spline()
    {
        std::vector<float3> spline;
        for (auto & p : lPoints) { spline.push_back(p.point); };
        return spline;
    }
    
};
    
}

#endif // constant_spline_h
