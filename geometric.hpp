// See COPYING file for attribution information

#ifndef geometric_h
#define geometric_h

#include "linear_algebra.hpp"
#include "math_util.hpp"
#include <ostream>

namespace avl
{
    
    /////////////////////////////////
    // Axis-Aligned Bounding Areas //
    /////////////////////////////////
    
    struct Bounds2D
    {
        float2 _min = {0, 0};
        float2 _max = {0, 0};
        
        Bounds2D() {}
        Bounds2D(float2 min, float2 max) : _min(min), _max(max) {}
        Bounds2D(float x0, float y0, float x1, float y1) { _min.x = x0; _min.y = y0; _max.x = x1; _max.y = y1; }
        
        float2 min() const { return _min; }
        float2 max() const { return _max; }
        
        float2 size() const { return max() - min(); }
        float2 center() const { return {(_min.x+_max.y)/2, (_min.y+_max.y)/2}; }
        float area() const { return (_max.x - _min.x) * (_max.y - _min.y); }
        
        float width() const { return _max.x - _min.x; }
        float height() const { return _max.y - _min.y; }
        
        bool contains(const float px, const float py) const { return px >= _min.x && py >= _min.y && px < _max.x && py < _max.y; }
        bool contains(const float2 & point) const { return contains(point.x, point.y); }
        
        bool intersects(const Bounds2D & other) const
        {
            if ((_min.x <= other._min.x) && (_max.x >= other._max.x) &&
                (_min.y <= other._min.y) && (_max.y >= other._max.y)) return true;
            return false;
        }
    };
    
    struct Bounds3D
    {
        float3 _min = {0, 0, 0};
        float3 _max = {0, 0, 0};
        
        Bounds3D() {}
        Bounds3D(float3 min, float3 max) : _min(min), _max(max) {}
        Bounds3D(float x0, float y0, float z0, float x1, float y1, float z1) { _min.x = x0; _min.y = y0; _min.z = z0; _max.x = x1; _max.y = y1; _max.z = z1; }
        
        float3 min() const { return _min; }
        float3 max() const { return _max; }
        
        float3 size() const { return _max - _min; }
        float3 center() const { return (_min + _max) * 0.5f; }
        float volume() const { return (_max.x - _min.x) * (_max.y - _min.y) * (_max.z - _min.z); }
        
        float width() const { return _max.x - _min.x; }
        float height() const { return _max.y - _min.y; }
        float depth() const { return _max.z - _min.z; }
        
        bool contains(float3 point) const
        {
            if (point.x < _min.x || point.x > _max.x) return false;
            if (point.y < _min.y || point.y > _max.y) return false;
            if (point.z < _min.z || point.z > _max.z) return false;
            return true;
        }
        
        bool intersects(const Bounds3D & other) const
        {
            if ((_min.x <= other._min.x) && (_max.x >= other._max.x) &&
                (_min.y <= other._min.y) && (_max.y >= other._max.y) &&
                (_min.z <= other._min.z) && (_max.z >= other._max.z)) return true;
            return false;
        }
        
    };
    
    
    /////////////////////////////////
    // Universal Coordinate System //
    /////////////////////////////////
    
    struct UCoord
    {
        float a, b;
        float resolve(float min, float max) const { return min + a * (max - min) + b; }
    };
    
    struct URect
    {
        UCoord x0, y0, x1, y1;
        Bounds2D resolve(const Bounds2D & r) const { return { x0.resolve(r.min().x, r.max().x), y0.resolve(r.min().y, r.max().y), x1.resolve(r.min().x, r.max().x), y1.resolve(r.min().y, r.max().y) }; }
        bool is_fixed_width() const { return x0.a == x1.a; }
        bool is_fixed_height() const { return y0.a == y1.a; }
        float fixed_width() const { return x1.b - x0.b; }
        float fixed_height() const { return y1.b - y0.b; }
    };
    
    ////////////////////////////////////
    // Construct rotation quaternions //
    ////////////////////////////////////
    
    inline float4 make_rotation_quat_axis_angle(const float3 & axis, float angle)
    {
        return {axis * std::sin(angle/2), std::cos(angle/2)};
    }
    
    inline float4 make_rotation_quat_around_x(float angle)
    {
        return make_rotation_quat_axis_angle({1,0,0}, angle);
    }
    
    inline float4 make_rotation_quat_around_y(float angle)
    {
        return make_rotation_quat_axis_angle({0,1,0}, angle);
    }
    
    inline float4 make_rotation_quat_around_z(float angle)
    {
        return make_rotation_quat_axis_angle({0,0,1}, angle);
    }
    
    inline float4 make_rotation_quat_between_vectors(const float3 & from, const float3 & to)
    {
        auto a = normalize(from), b = normalize(to);
        return make_rotation_quat_axis_angle(normalize(cross(a,b)), std::acos(dot(a,b)));
    }
    
    inline float4 make_rotation_quat_between_vectors_snapped(const float3 & from, const float3 & to, const float angle)
    {
        auto a = normalize(from);
        auto b = normalize(to);
        auto snappedAcos = std::floor(std::acos(dot(a,b)) / angle) * angle;
        return make_rotation_quat_axis_angle(normalize(cross(a,b)), snappedAcos);
    }
    
    inline float4 make_rotation_quat_from_rotation_matrix(const float3x3 & m)
    {
        const float magw =  m(0,0) + m(1,1) + m(2,2);
        
        const bool wvsz = magw  > m(2,2);
        const float magzw  = wvsz ? magw : m(2,2);
        const float3 prezw  = wvsz ? float3(1,1,1) : float3(-1,-1,1) ;
        const float4 postzw = wvsz ? float4(0,0,0,1): float4(0,0,1,0);
        
        const bool xvsy = m(0,0) > m(1,1);
        const float magxy = xvsy ? m(0,0) : m(1,1);
        const float3 prexy = xvsy ? float3(1,-1,-1) : float3(-1,1,-1) ;
        const float4 postxy = xvsy ? float4(1,0,0,0) : float4(0,1,0,0);
        
        const bool zwvsxy = magzw > magxy;
        const float3 pre  = zwvsxy ? prezw  : prexy ;
        const float4 post = zwvsxy ? postzw : postxy;
        
        const float t = pre.x * m(0,0) + pre.y * m(1,1) + pre.z * m(2,2) + 1;
        const float s = 1.f / sqrt(t) / 2.f;
        const float4 qp = float4(pre.y * m(2,1) - pre.z * m(1,2), pre.z * m(0,2) - pre.x * m(2,0), pre.x * m(1,0) - pre.y * m(0,1), t) * s;
        return qmul(qp, post);
    }
    
    inline float4 make_rotation_quat_from_pose_matrix(const float4x4 & m)
    {
        return make_rotation_quat_from_rotation_matrix({m.x.xyz(),m.y.xyz(),m.z.xyz()});
    }
    
    inline float4 make_axis_angle_rotation_quat(const float4 & q)
    {
        float w = 2.0f * (float) acosf(std::max(-1.0f, std::min(q.w, 1.0f))); // angle
        float den = (float) sqrt(std::abs(1.0 - q.w * q.w));
        if (den > 1E-5f) return {q.x / den, q.y / den, q.z / den, w};
        return {1.0f, 0.f, 0.f, w};
    }
    
    inline float4 make_quat_from_euler(float roll, float pitch, float yaw)
    {
        double sy = sin(yaw * 0.5);
        double cy = cos(yaw * 0.5);
        double sp = sin(pitch * 0.5);
        double cp = cos(pitch * 0.5);
        double sr = sin(roll * 0.5);
        double cr = cos(roll * 0.5);
        double w = cr*cp*cy + sr*sp*sy;
        double x = sr*cp*cy - cr*sp*sy;
        double y = cr*sp*cy + sr*cp*sy;
        double z = cr*cp*sy - sr*sp*cy;
        return float4(x,y,z,w);
    }
    
    inline float3 make_euler_from_quat(float4 q)
    {
        float3 e;
        const double q0 = q.w;
        const double q1 = q.x;
        const double q2 = q.y;
        const double q3 = q.z;
        e.x = atan2(2*(q0*q1+q2*q3), 1-2*(q1*q1+q2*q2));
        e.y = asin(2*(q0*q2-q3*q1));
        e.z = atan2(2*(q0*q3+q1*q2), 1-2*(q2*q2+q3*q3));
        return e;
    }

    // Decompose rotation of q around the axis vt where q = swing * twist
    // Twist is a rotation about vt, and swing is a rotation about a vector perpindicular to vt
    // http://www.alinenormoyle.com/weblog/?p=726.
    // A singularity exists when swing is close to 180 degrees.
    inline void decompose_swing_twist(const float4 q, const float3 vt, float4 & swing, float4 & twist)
    {
        float3 p = vt * dot(vt, twist.xyz());
        twist = normalize(float4(p.x, p.y, p.z, q.w));
        if (!twist.x && !twist.y && !twist.z && !twist.w) twist = float4(0, 0, 0, 1); // singularity
        swing = q * qconj(twist);
    }
    
    
    //////////////////////////////////////////////
    // Construct affine transformation matrices //
    //////////////////////////////////////////////
    
    struct Pose;
    
    inline float4x4 make_scaling_matrix(float scaling)
    {
        return {{scaling,0,0,0}, {0,scaling,0,0}, {0,0,scaling,0}, {0,0,0,1}};
    }
    
    inline float4x4 make_scaling_matrix(const float3 & scaling)
    {
        return {{scaling.x,0,0,0}, {0,scaling.y,0,0}, {0,0,scaling.z,0}, {0,0,0,1}};
    }
    
    inline float4x4 make_rotation_matrix(const float4 & rotation)
    {
        return {{qxdir(rotation),0},{qydir(rotation),0},{qzdir(rotation),0},{0,0,0,1}};
    }
    
    inline float4x4 make_rotation_matrix(const float3 & axis, float angle)
    {
        return make_rotation_matrix(make_rotation_quat_axis_angle(axis, angle));
    }
    
    inline float4x4 make_translation_matrix(const float3 & translation)
    {
        return {{1,0,0,0}, {0,1,0,0}, {0,0,1,0}, {translation,1}};
    }
    
    inline float4x4 make_rigid_transformation_matrix(const float4 & rotation, const float3 & translation)
    {
        return {{qxdir(rotation),0},{qydir(rotation),0},{qzdir(rotation),0},{translation,1}};
    }
    
    inline float4x4 make_view_matrix_from_pose(const Pose & pose); // defined below
    
    inline float4x4 make_projection_matrix(float l, float r, float b, float t, float n, float f)
    {
        return {{2*n/(r-l),0,0,0}, {0,2*n/(t-b),0,0}, {(r+l)/(r-l),(t+b)/(t-b),-(f+n)/(f-n),-1}, {0,0,-2*f*n/(f-n),0}};
    }
    
    inline float4x4 make_perspective_matrix(float vFovInRadians, float aspectRatio, float nearZ, float farZ)
    {
        const float top = nearZ * std::tan(vFovInRadians/2.f), right = top * aspectRatio;
        return make_projection_matrix(-right, right, -top, top, nearZ, farZ);
    }
    
    inline float4x4 make_orthographic_matrix(float l, float r, float b, float t, float n, float f)
    {
        return {{2/(r-l),0,0,0}, {0,2/(t-b),0,0}, {0,0,-2/(f-n),0}, {-(r+l)/(r-l),-(t+b)/(t-b),-(f+n)/(f-n),1}};
    }
    
    //     | 1-2Nx^2   -2NxNy  -2NxNz  -2NxD |
    // m = |  -2NxNy  1-2Ny^2  -2NyNz  -2NyD |
    //     |  -2NxNz   -2NyNz 1-2Nz^2  -2NzD |
    //     |    0        0       0       1   |
    // Where (Nx,Ny,Nz,D) are the coefficients of plane equation (xNx + yNy + zNz + D = 0) and
    // (Nx, Ny, Nz) is the normal vector of given plane.
    inline float4x4 make_reflection_matrix(const float4 plane)
    {
        float4x4 reflectionMat = Zero4x4;
        
        reflectionMat(0,0) = (1.f-2.0f * plane[0]*plane[0]);
        reflectionMat(0,1) = (   -2.0f * plane[0]*plane[1]);
        reflectionMat(0,2) = (   -2.0f * plane[0]*plane[2]);
        reflectionMat(0,3) = (   -2.0f * plane[3]*plane[0]);
        
        reflectionMat(1,0) = (   -2.0f * plane[1]*plane[0]);
        reflectionMat(1,1) = (1.f-2.0f * plane[1]*plane[1]);
        reflectionMat(1,2) = (   -2.0f * plane[1]*plane[2]);
        reflectionMat(1,3) = (   -2.0f * plane[3]*plane[1]);
        
        reflectionMat(2,0) = (   -2.0f * plane[2]*plane[0]);
        reflectionMat(2,1) = (   -2.0f * plane[2]*plane[1]);
        reflectionMat(2,2) = (1.f-2.0f * plane[2]*plane[2]);
        reflectionMat(2,3) = (   -2.0f * plane[3]*plane[2]);
        
        reflectionMat(3,0) = 0.0f;
        reflectionMat(3,1) = 0.0f;
        reflectionMat(3,2) = 0.0f;
        reflectionMat(3,3) = 1.0f;
        
        return reflectionMat;
    }
    
    inline float3x3 get_rotation_submatrix(const float4x4 & transform)
    {
        return {transform.x.xyz(), transform.y.xyz(), transform.z.xyz()};
    }
    
    inline float3 transform_coord(const float4x4 & transform, const float3 & coord)
    {
        auto r = mul(transform, float4(coord,1)); return (r.xyz() / r.w);
    }
    
    inline float3 transform_vector(const float4x4 & transform, const float3 & vector)
    {
        return mul(transform, float4(vector,0)).xyz();
    }
    
    inline float3 transform_vector(const float4 & b, const float3 & a)
    {
        return qmul(b, float4(a, 1)).xyz();
    }
    
    ///////////
    // Poses //
    ///////////
    
    struct Pose
    {
        float4      orientation;                                    // Orientation of an object, expressed as a rotation quaternion from the base orientation
        float3      position;                                       // Position of an object, expressed as a translation vector from the base position
        
        Pose()                                                      : Pose({0,0,0,1}, {0,0,0}) {}
        Pose(const float4 & orientation, const float3 & position)   : orientation(orientation), position(position) {}
        explicit    Pose(const float4 & orientation)                : Pose(orientation, {0,0,0}) {}
        explicit    Pose(const float3 & position)                   : Pose({0,0,0,1}, position) {}
        
        float4x4    matrix() const                                  { return make_rigid_transformation_matrix(orientation, position); }
        float3      xdir() const                                    { return qxdir(orientation); } // Equivalent to transform_vector({1,0,0})
        float3      ydir() const                                    { return qydir(orientation); } // Equivalent to transform_vector({0,1,0})
        float3      zdir() const                                    { return qzdir(orientation); } // Equivalent to transform_vector({0,0,1})
        Pose        inverse() const                                 { auto invOri = qinv(orientation); return {invOri, qrot(invOri, -position)}; }
        
        float3      transform_vector(const float3 & vec) const      { return qrot(orientation, vec); }
        float3      transform_coord(const float3 & coord) const     { return position + transform_vector(coord); }
        float3      detransform_coord(const float3 & coord) const   { return detransform_vector(coord - position); }    // Equivalent to inverse().transform_coord(coord), but faster
        float3      detransform_vector(const float3 & vec) const    { return qrot(qinv(orientation), vec); }            // Equivalent to inverse().transform_vector(vec), but faster
        
        Pose        operator * (const Pose & pose) const            { return {qmul(orientation,pose.orientation), transform_coord(pose.position)}; }
    };
    
    inline float4x4 make_view_matrix_from_pose(const Pose & pose)
    {
        return pose.inverse().matrix();
    }
    
    inline Pose look_at_pose(float3 eyePoint, float3 target, float3 worldUp = {0,1,0})
    {
        Pose p;
        float3 zDir = normalize(eyePoint - target);
        float3 xDir = normalize(cross(worldUp, zDir));
        float3 yDir = cross(zDir, xDir);
        p.position = eyePoint;
        p.orientation = normalize(make_rotation_quat_from_rotation_matrix({xDir, yDir, zDir}));
        return p;
    }
    
    /////////////
    //   Ray   //
    /////////////
    
    struct Ray
    {
        float3 origin;
        float3 direction;
        
        Ray() {}
        Ray(const float3 & ori, const float3 & dir) : origin(ori), direction(dir) {}
        
        float3 inverse_direction() const { return {1.f / direction.x, 1.f / direction.y, 1.f / direction.z }; }
        
        void transform(const float4x4 & matrix)
        {
            origin = transform_vector(matrix, origin);
            direction = get_rotation_submatrix(matrix) * direction;
        }
        
        Ray transformed(const float4x4 & matrix) const
        {
            Ray result;
            result.origin = transform_vector(matrix, origin);
            result.direction = get_rotation_submatrix(matrix) * direction;
            return result;
        }
        
        float3 calculate_position(float t) const { return origin + direction * t; }
    };
    
    inline std::ostream & operator << (std::ostream & o, const Ray & r)
    {
        return o << "{" << r.origin << " => " << r.direction << "}";
    }
    
    inline Ray operator * (const Pose & pose, const Ray & ray)
    {
        return {pose.transform_coord(ray.origin), pose.transform_vector(ray.direction)};
    }
    
    inline Ray between(const float3 & start, const float3 & end)
    {
        return {start, normalize(end - start)};
    }
    
    inline Ray ray_from_viewport_pixel(const float2 & pixelCoord, const float2 & viewportSize, const float4x4 & projectionMatrix)
    {
        float vx = pixelCoord.x * 2 / viewportSize.x - 1, vy = 1 - pixelCoord.y * 2 / viewportSize.y;
        auto invProj = inv(projectionMatrix);
        return {{0,0,0}, normalize(transform_coord(invProj, {vx, vy, +1}) - transform_coord(invProj, {vx, vy, -1}))};
    }
    
}

#endif // end geometric_h
