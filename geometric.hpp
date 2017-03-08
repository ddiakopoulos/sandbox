// See COPYING file for attribution information

#ifndef geometric_h
#define geometric_h

#include "linalg_util.hpp"
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
    
    inline std::ostream & operator << (std::ostream & o, const Bounds2D & b)
    {
        return o << "{" << b.min() << " to " << b.max() << "}";
    }

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

        uint32_t maximum_extent() const
        {
            auto d = _max - _min;
            if (d.x > d.y && d.x > d.z) return 0;
            else if (d.y > d.z) return 1;
            else return 2;
        }

        Bounds3D add(const Bounds3D & other) const
        {
            Bounds3D result;
            result._min = linalg::min(_min, other._min);
            result._max = linalg::max(_max, other._max);
            return result;
        }
    };

    inline std::ostream & operator << (std::ostream & o, const Bounds3D & b)
    {
        return o << "{" << b.min() << " to " << b.max() << "}";
    }

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
    
    /////////////////////////////
    // General 3D Math Helpers //
    /////////////////////////////

    inline float3 reflect(const float3 & I, const float3 & N)
    {
        return I - (N * dot(N, I) * 2.f);
    }

    inline float3 refract(const float3 & I, const float3 & N, float eta)
    {
        float k = 1.0f - eta * eta * (1.0f - dot(N, I) * dot(N, I));
        if (k < 0.0f) return float3();
        else return eta * I - (eta * dot(N, I) + std::sqrt(k)) * N;
    }

    inline float3 faceforward(const float3 & N, const float3 & I, const float3 & Nref)
    {
        return dot(Nref, I) < 0.f ? N : -N;
    }

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
    
    // http://lolengine.net/blog/2013/09/18/beautiful-maths-quaternion-from-vectors
    inline float4 make_rotation_quat_between_vectors(const float3 & from, const float3 & to)
    {
        auto a = safe_normalize(from), b = safe_normalize(to);
        return make_rotation_quat_axis_angle(safe_normalize(cross(a,b)), std::acos(dot(a,b)));
    }
    
    inline float4 make_rotation_quat_between_vectors_snapped(const float3 & from, const float3 & to, const float angle)
    {
        auto a = safe_normalize(from);
        auto b = safe_normalize(to);
        auto snappedAcos = std::floor(std::acos(dot(a,b)) / angle) * angle;
        return make_rotation_quat_axis_angle(safe_normalize(cross(a,b)), snappedAcos);
    }
    
    inline float4 make_rotation_quat_from_rotation_matrix(const float3x3 & m)
    {
        const float magw =  m[0][0] + m[1][1] + m[2][2];
        
        const bool wvsz = magw  > m[2][2];
        const float magzw  = wvsz ? magw : m[2][2];
        const float3 prezw  = wvsz ? float3(1,1,1) : float3(-1,-1,1) ;
        const float4 postzw = wvsz ? float4(0,0,0,1): float4(0,0,1,0);
        
        const bool xvsy = m[0][0] > m[1][1];
        const float magxy = xvsy ? m[0][0] : m[1][1];
        const float3 prexy = xvsy ? float3(1,-1,-1) : float3(-1,1,-1) ;
        const float4 postxy = xvsy ? float4(1,0,0,0) : float4(0,1,0,0);
        
        const bool zwvsxy = magzw > magxy;
        const float3 pre  = zwvsxy ? prezw  : prexy ;
        const float4 post = zwvsxy ? postzw : postxy;
        
        const float t = pre.x * m[0][0] + pre.y * m[1][1] + pre.z * m[2][2] + 1;
        const float s = 1.f / sqrt(t) / 2.f;
        const float4 qp = float4(pre.y * m[1][2] - pre.z * m[2][1], pre.z * m[2][0] - pre.x * m[0][2], pre.x * m[0][1] - pre.y * m[1][0], t) * s;
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
    
    //////////////////////////
    // Quaternion Utilities //
    //////////////////////////
    
    // Quaternion <=> Euler ref: http://www.swarthmore.edu/NatSci/mzucker1/e27/diebel2006attitude.pdf
    // ZYX is probably the most common standard: yaw, pitch, roll (YPR)
    // XYZ Somewhat less common: roll, pitch, yaw (RPY)

    inline float4 make_quat_from_euler_zyx(float y, float p, float r)
    {
        float4 q;
        q.x = cos(y/2.f)*cos(p/2.f)*cos(r/2.f) - sin(y/2.f)*sin(p/2.f)*sin(r/2.f);
        q.y = cos(y/2.f)*cos(p/2.f)*sin(r/2.f) + sin(y/2.f)*cos(r/2.f)*sin(p/2.f);
        q.z = cos(y/2.f)*cos(r/2.f)*sin(p/2.f) - sin(y/2.f)*cos(p/2.f)*sin(r/2.f);
        q.w = cos(y/2.f)*sin(p/2.f)*sin(r/2.f) + cos(p/2.f)*cos(r/2.f)*sin(y/2.f);
        return q;
    }

    inline float4 make_quat_from_euler_xyz(float r, float p, float y)
    {
        float4 q;
        q.x = cos(r/2.f)*cos(p/2.f)*cos(y/2.f) + sin(r/2.f)*sin(p/2.f)*sin(y/2.f);
        q.y = sin(r/2.f)*cos(p/2.f)*cos(y/2.f) - cos(r/2.f)*sin(y/2.f)*sin(p/2.f);
        q.z = cos(r/2.f)*cos(y/2.f)*sin(p/2.f) + sin(r/2.f)*cos(p/2.f)*sin(y/2.f);
        q.w = cos(r/2.f)*cos(p/2.f)*sin(y/2.f) - sin(p/2.f)*cos(y/2.f)*sin(r/2.f);
        return q;
    }

    inline float3 make_euler_from_quat_zyx(float4 q)
    {
        float3 ypr;
        const double q0 = q.w, q1 = q.x, q2 = q.y, q3 = q.z;
        ypr.x = float(atan2(-2.f*q1*q2 + 2 * q0*q3, q1*q1 + q0*q0 - q3*q3 - q2*q2));
        ypr.y = float(asin(2.f*q1*q3 + 2.f*q0*q2));
        ypr.z = float(atan2(-2.f*q2*q3 + 2.f*q0*q1, q3*q3 - q2*q2 - q1*q1 + q0*q0));
        return ypr;
    }

    inline float3 make_euler_from_quat_xyz(float4 q)
    {
        float3 rpy;
        const double q0 = q.w, q1 = q.x, q2 = q.y, q3 = q.z;
        rpy.x = float(atan2(2.f*q2*q3 + 2.f*q0*q1, q3*q3 - q2*q2 - q1*q1 + q0*q0));
        rpy.y = float(-asin(2.f*q1*q3 - 2.f*q0*q2));
        rpy.z = float(atan2(2.f*q1*q2 + 2.f*q0*q3, q1*q1 + q0*q0 - q3*q3 - q2*q2));
        return rpy;
    }

    // Decompose rotation of q around the axis vt where q = swing * twist
    // Twist is a rotation about vt, and swing is a rotation about a vector perpindicular to vt
    // http://www.alinenormoyle.com/weblog/?p=726.
    // A singularity exists when swing is close to 180 degrees.
    inline void decompose_swing_twist(const float4 q, const float3 vt, float4 & swing, float4 & twist)
    {
        float3 p = vt * dot(vt, twist.xyz());
        twist = safe_normalize(float4(p.x, p.y, p.z, q.w));
        if (!twist.x && !twist.y && !twist.z && !twist.w) twist = float4(0, 0, 0, 1); // singularity
        swing = q * qconj(twist);
    }

    inline float4 interpolate_short(const float4 & a, const float4 & b, const float & t)
    {
        if (t <= 0) return a;
        if (t >= 0) return b;

        float fCos = dot(a, b);
        auto b2(b);

        if (fCos < 0)
        {
            b2 = -b;
            fCos = -fCos;
        }

        float k0, k1;
        if (fCos >(1.f - std::numeric_limits<float>::epsilon()))
        {
            k0 = 1.f - t;
            k1 = t;
        }
        else
        {
            const float s = std::sqrt(1.f - fCos * fCos), ang = std::atan2(s, fCos), oneOverS = 1.f / s;
            k0 = (std::sin(1.f - t) * ang) * oneOverS;
            k1 = std::sin(t * ang) * oneOverS;
        }

        return{ k0 * a.x + k1 * b2.x, k0 * a.y + k1 * b2.y, k0 * a.z + k1 * b2.z, k0 * a.w + k1 * b2.w };
    }

    inline float compute_quat_closeness(const float4 & a, const float4 & b)
    {
        return std::acos((2.f * pow(dot(a, b), 2.f))  - 1.f);
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
        static const avl::float4x4 Zero4x4 = {{0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}};
        float4x4 reflectionMat = Zero4x4;
        
        reflectionMat[0][0] = (1.f-2.0f * plane[0]*plane[0]);
        reflectionMat[1][0] = (   -2.0f * plane[0]*plane[1]);
        reflectionMat[2][0] = (   -2.0f * plane[0]*plane[2]);
        reflectionMat[3][0] = (   -2.0f * plane[3]*plane[0]);

        reflectionMat[0][1] = (   -2.0f * plane[1]*plane[0]);
        reflectionMat[1][1] = (1.f-2.0f * plane[1]*plane[1]);
        reflectionMat[2][1] = (   -2.0f * plane[1]*plane[2]);
        reflectionMat[3][1] = (   -2.0f * plane[3]*plane[1]);

        reflectionMat[0][2] = (   -2.0f * plane[2]*plane[0]);
        reflectionMat[1][2] = (   -2.0f * plane[2]*plane[1]);
        reflectionMat[2][2] = (1.f-2.0f * plane[2]*plane[2]);
        reflectionMat[3][2] = (   -2.0f * plane[3]*plane[2]);

        reflectionMat[0][3] = 0.0f;
        reflectionMat[1][3] = 0.0f;
        reflectionMat[2][3] = 0.0f;
        reflectionMat[3][3] = 1.0f;
        
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

    // Rigid transformation value-type
    struct Pose
    {
        float4      orientation;                                    // Orientation of an object, expressed as a rotation quaternion from the base orientation
        float3      position;                                       // Position of an object, expressed as a translation vector from the base position
        
        Pose()                                                      : Pose({0,0,0,1}, {0,0,0}) {}
        Pose(const float4 & orientation, const float3 & position)   : orientation(orientation), position(position) {}
        explicit    Pose(const float4 & orientation)                : Pose(orientation, {0,0,0}) {}
        explicit    Pose(const float3 & position)                   : Pose({0,0,0,1}, position) {}
        
        Pose        inverse() const                                 { auto invOri = qinv(orientation); return{ invOri, qrot(invOri, -position) }; }
        float4x4    matrix() const                                  { return make_rigid_transformation_matrix(orientation, position); }
        float3      xdir() const                                    { return qxdir(orientation); } // Equivalent to transform_vector({1,0,0})
        float3      ydir() const                                    { return qydir(orientation); } // Equivalent to transform_vector({0,1,0})
        float3      zdir() const                                    { return qzdir(orientation); } // Equivalent to transform_vector({0,0,1})
        
        float3      transform_vector(const float3 & vec) const      { return qrot(orientation, vec); }
        float3      transform_coord(const float3 & coord) const     { return position + transform_vector(coord); }
        float3      detransform_coord(const float3 & coord) const   { return detransform_vector(coord - position); }    // Equivalent to inverse().transform_coord(coord), but faster
        float3      detransform_vector(const float3 & vec) const    { return qrot(qinv(orientation), vec); }            // Equivalent to inverse().transform_vector(vec), but faster
        
        Pose        operator * (const Pose & pose) const            { return {qmul(orientation,pose.orientation), transform_coord(pose.position)}; }
    };
    
    inline bool operator == (const Pose & a, const Pose & b)
    {
        return (a.position == b.position) && (a.orientation == b.orientation);
    }

    inline bool operator != (const Pose & a, const Pose & b)
    {
        return (a.position != b.position) || (a.orientation != b.orientation);
    }

    inline std::ostream & operator << (std::ostream & o, const Pose & r)
    {
        return o << "{" << r.position << ", " << r.orientation << "}";
    }

    // The long form of (a.inverse() * b) 
    inline Pose make_pose_from_to(const Pose & a, const Pose & b)
    {
        Pose ret;
        const auto inv = qinv(a.orientation);
        ret.orientation = qmul(inv, b.orientation);
        ret.position = qrot(inv, b.position - a.position);
        return ret;
    }

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

    inline Pose make_pose_from_transform_matrix(float4x4 transform)
    {
        Pose p;
        p.position = transform[3].xyz();
        p.orientation = make_rotation_quat_from_rotation_matrix(get_rotation_submatrix(transform));
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
            direction = mul(get_rotation_submatrix(matrix), direction);
        }
        
        Ray transformed(const float4x4 & matrix) const
        {
            Ray result;
            result.origin = transform_vector(matrix, origin);
            result.direction = mul(get_rotation_submatrix(matrix), direction);
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
        return {start, safe_normalize(end - start)};
    }
    
    inline Ray ray_from_viewport_pixel(const float2 & pixelCoord, const float2 & viewportSize, const float4x4 & projectionMatrix)
    {
        float vx = pixelCoord.x * 2 / viewportSize.x - 1, vy = 1 - pixelCoord.y * 2 / viewportSize.y;
        auto invProj = inv(projectionMatrix);
        return {{0,0,0}, safe_normalize(transform_coord(invProj, {vx, vy, +1}) - transform_coord(invProj, {vx, vy, -1}))};
    }

    ////////////////
    //   Sphere   //
    ////////////////

    static const double SPHERE_EPSILON = 0.0001;
    
    struct Sphere
    {
        float3 center;
        float radius;
        
        Sphere() {}
        Sphere(const float3 & center, float radius) : center(center), radius(radius) {}
        
        // Returns the closest point on the ray to the Sphere. If ray intersects then returns the point of nearest intersection.
        float3 closest_point(const Ray & ray) const
        {
            float t;
            float3 diff = ray.origin - center;
            float a = dot(ray.direction, ray.direction);
            float b = 2.0f * dot(diff, ray.direction);
            float c = dot(diff, diff) - radius * radius;
            float disc = b * b - 4.0f * a * c;
            
            if (disc > 0)
            {
                float e = std::sqrt(disc);
                float denom = 2 * a;
                t = (-b - e) / denom;    // smaller root
                
                if (t > SPHERE_EPSILON) return ray.calculate_position(t);
                
                t = (-b + e) / denom;    // larger root
                if (t > SPHERE_EPSILON) return ray.calculate_position(t);
            }
            
            // doesn't intersect; closest point on line
            t = dot( -diff, safe_normalize(ray.direction) );
            float3 onRay = ray.calculate_position(t);
            return center + safe_normalize( onRay - center ) * radius;
        }
        
        // Converts sphere to another coordinate system. Note that it will not return correct results if there are non-uniform scaling, shears, or other unusual transforms.
        Sphere transformed(const float4x4 & transform)
        {
            float4 tCenter = mul(transform, float4(center, 1));
            float4 tRadius = mul(transform, float4(radius, 0, 0, 0));
            return Sphere(float3(tCenter.x, tCenter.y, tCenter.z), length(tRadius));
        }
        
        void calculate_projection(float focalLength, float2 *outCenter, float2 *outAxisA, float2 *outAxisB) const
        {
            float3 o(-center.x, center.y, center.z);
            
            float r2 = radius * radius;
            float z2 = o.z * o.z;
            float l2 = dot(o, o);
            
            if (outCenter) *outCenter = focalLength * o.z * float2(o.x, o.y) / (z2-r2);
            
            if (fabs(z2 - l2) > 0.00001f)
            {
                if (outAxisA) *outAxisA = focalLength * sqrtf(-r2*(r2-l2)/((l2-z2)*(r2-z2)*(r2-z2))) * float2(o.x, o.y);
                if (outAxisB) *outAxisB = focalLength * sqrtf(fabs(-r2*(r2-l2)/((l2-z2)*(r2-z2)*(r2-l2)))) * float2(-o.y, o.x);
            }
            
            // approximate with circle
            else
            {
                float newRadius = focalLength * radius / sqrtf(z2 - r2);
                if (outAxisA) *outAxisA = float2(newRadius, 0);
                if (outAxisB) *outAxisB = float2(0, newRadius);
            }
        }
        
        // Calculates the projection of the sphere (an oriented ellipse) given a focal length. Algorithm due to Iñigo Quilez.
        void calculate_projection(float focalLength, float2 screenSizePixels, float2 * outCenter, float2 * outAxisA, float2 * outAxisB) const
        {
            auto toScreenPixels = [=] (float2 v, const float2 &winSizePx) {
                float2 result = v;
                result.x *= 1 / (winSizePx.x / winSizePx.y);
                result += float2(0.5f);
                result *= winSizePx;
                return result;
            };
            
            float2 center, axisA, axisB;
            
            calculate_projection(focalLength, &center, &axisA, &axisB);
            if (outCenter) *outCenter = toScreenPixels(center, screenSizePixels);
            if (outAxisA) *outAxisA = toScreenPixels(center + axisA * 0.5f, screenSizePixels) - toScreenPixels(center - axisA * 0.5f, screenSizePixels);
            if (outAxisB) *outAxisB = toScreenPixels(center + axisB * 0.5f, screenSizePixels) - toScreenPixels(center - axisB * 0.5f, screenSizePixels);
        }
        
    };

    ///////////////
    //   Plane   //
    ///////////////
    
    static const double PLANE_EPSILON = 0.0001;
    
    struct Plane
    {
        float4 equation = { 0, 0, 0, 0 }; // ax * by * cz + d form (xyz normal, w distance)
        Plane() {}
        Plane(const float4 & equation) : equation(equation) {}
        Plane(const float3 & normal, const float & distance) { equation = float4(normal.x, normal.y, normal.z, distance); }
        Plane(const float3 & normal, const float3 & point) { equation = float4(normal.x, normal.y, normal.z, -dot(normal, point)); }
        float3 get_normal() const { return equation.xyz(); }
        bool is_negative_half_space(const float3 & point) const { return (dot(get_normal(), point) < -equation.w); }; // +eq.w?
        void normalize() { float n = 1.0f / length(get_normal()); equation *= n; };
        float get_distance() const { return equation.w; }
        float distance_to(float3 point) const { return dot(get_normal(), point) + equation.w; };
        bool contains(float3 point) const { return std::abs(distance_to(point)) < PLANE_EPSILON; };
    };

    /////////////////
    //   Segment   //
    /////////////////
    
    struct Segment
    {
        float3 first, second;
        Segment(float3 first, float3 second) : first(first), second(second) {}
        float3 get_direction() const { return safe_normalize (second - first); };
    };
    
    //////////////
    //   Line   //
    //////////////

    struct Line
    {
        float3 point, direction;
        Line(const float3 & pt, const float3 & dir) : point(pt), direction(dir) {}
    };
    
    /////////////////////////////////
    // Object-Object intersections //
    /////////////////////////////////
    
    // http://paulbourke.net/geometry/pointlineplane/
    inline Line intersect_plane_plane(const Plane & p1, const Plane & p2)
    {
        const float ndn = dot(p1.get_normal(), p2.get_normal());
        const float recDeterminant = 1.f / (1.f - (ndn * ndn));
        const float c1 = (-p1.get_distance() + (p2.get_distance() * ndn)) * recDeterminant;
        const float c2 = (-p2.get_distance() + (p1.get_distance() * ndn)) * recDeterminant;
        return Line((c1 * p1.get_normal()) + (c2 * p2.get_normal()), normalize(cross(p1.get_normal(), p2.get_normal())));
    }

    inline float3 intersect_line_plane(const Line & l, const Plane & p)
    {
        const float d = dot(l.direction, p.get_normal());
        const float dist = p.distance_to(l.point) / d;
        return (l.point - (dist * l.direction));
    }
    
    //////////////////////////////
    // Ray-object intersections //
    //////////////////////////////
    
    inline bool intersect_ray_plane(const Ray & ray, const Plane & p, float3 * intersection = nullptr, float * outT = nullptr)
    {
        const float d = dot(ray.direction, p.get_normal());

        // Make sure we're not parallel to the plane
        if (std::abs(d) > PLANE_EPSILON)
        {
            const float t = -p.distance_to(ray.origin) / d;
            
            if (t >= PLANE_EPSILON)
            {
                if (outT) *outT = t;
                if (intersection) *intersection = ray.origin + (t * ray.direction);
                return true;
            }
        }
        if (outT) *outT = std::numeric_limits<float>::max();
        return false;
    }
    
    // Real-Time Collision Detection pg. 180
    inline bool intersect_ray_box(const Ray & ray, const Bounds3D bounds, float * outTmin = nullptr, float * outTmax = nullptr, float3 * outNormal = nullptr)
    {
        float tmin = 0.f; // set to -FLT_MAX to get first hit on line
        float tmax = std::numeric_limits<float>::max(); // set to max distance ray can travel (for segment)
        float3 n;
        float3 normal(0, 0, 0);

        const float3 invDist = ray.inverse_direction();

        // For all three slabs
        for (int i = 0; i < 3; ++i)
        {
            if (std::abs(ray.direction[i]) < PLANE_EPSILON)
            {
                // Ray is parallel to slab. No hit if r.origin not within slab
                if ((ray.origin[i] < bounds.min()[i]) || (ray.origin[i] > bounds.max()[i])) return false;
            }
            else
            {
                // Compute intersection t value of ray with near and far plane of slab
                float t1 = (bounds.min()[i] - ray.origin[i]) * invDist[i]; // near
                float t2 = (bounds.max()[i] - ray.origin[i]) * invDist[i]; // far

                n.x = (i == 0) ? bounds.min()[i] : 0.0f;
                n.y = (i == 1) ? bounds.min()[i] : 0.0f;
                n.z = (i == 2) ? bounds.min()[i] : 0.0f;

                // Swap t1 and t2 if need so that t1 is intersection with near plane and t2 with far plane
                if (t1 > t2)
                {
                    std::swap(t1, t2);
                    n = -n;
                }

                // Compute the intersection of the of slab intersection interval with previous slabs
                if (t1 > tmin)
                {
                    tmin = t1;
                    normal = n;
                }
                tmax = std::min(tmax, t2);

                // If the slabs intersection is empty, there is no hit
                if (tmin > tmax || tmax <= PLANE_EPSILON) return false;
            }
        }
        if (outTmin) *outTmin = tmin;
        if (outTmax) *outTmax = tmax;
        if (outNormal) *outNormal = (tmin) ? normalize(normal) : float3(0, 0, 0);

        return true;
    }

    inline bool intersect_ray_sphere(const Ray & ray, const Sphere & sphere, float * outT = nullptr, float3 * outNormal = nullptr)
    {
        float t;
        const float3 diff = ray.origin - sphere.center;
        const float a = dot(ray.direction, ray.direction);
        const float b = 2.0f * dot(diff, ray.direction);
        const float c = dot(diff, diff) - sphere.radius * sphere.radius;
        const float disc = b * b - 4.0f * a * c;
        
        if (disc < 0.0f)
        {
            return false; 
        }
        else
        {
            float e = std::sqrt(disc);
            float denom = 1.f / (2.0f * a);
            
            t = (-b - e) * denom;
            if (t > SPHERE_EPSILON)
            {
                if (outT) *outT = t;
                if (outNormal) *outNormal = t ? ((ray.direction * t - diff) / sphere.radius) : normalize(ray.direction * t - diff);
                return true;
            }
            
            t = (-b + e) * denom;
            if (t > SPHERE_EPSILON)
            {
                if (outT) *outT = t;
                if (outNormal) *outNormal = t ? ((ray.direction * t - diff) / sphere.radius) : normalize(ray.direction * t - diff);
                return true;
            }
        }
        if (outT) *outT = 0;
        if (outNormal) *outNormal = float3(0, 0, 0);
        return false;
    }
    
    // Implementation adapted from: http://www.lighthouse3d.com/tutorials/maths/ray-triangle-intersection/
    inline bool intersect_ray_triangle(const Ray & ray, const float3 & v0, const float3 & v1, const float3 & v2, float * outT = nullptr, float2 * outUV = nullptr)
    {
        float3 e1 = v1 - v0, e2 = v2 - v0, h = cross(ray.direction, e2);
        
        float a = dot(e1, h);
        if (fabsf(a) == 0.0f) return false; // Ray is collinear with triangle plane
        
        float3 s = ray.origin - v0;
        float f = 1 / a;
        float u = f * dot(s,h);
        if (u < 0 || u > 1) return false; // Line intersection is outside bounds of triangle
        
        float3 q = cross(s, e1);
        float v = f * dot(ray.direction, q);
        if (v < 0 || u + v > 1) return false; // Line intersection is outside bounds of triangle
        
        float t = f * dot(e2, q);
        if (t < 0) return false; // Line intersection, but not a ray intersection
        
        if (outT) *outT = t;
        if (outUV) *outUV = {u,v};
        
        return true;
    }

    enum FrustumPlane
    {
        RIGHT,
        LEFT,
        BOTTOM,
        TOP,
        NEAR,
        FAR
    };

    struct Frustum
    {
        Plane planes[6];

        Frustum()
        {
            planes[FrustumPlane::RIGHT] = Plane({-1, 0, 0}, 1.f );
            planes[FrustumPlane::LEFT] = Plane({1, 0, 0 }, 1.f);
            planes[FrustumPlane::BOTTOM] = Plane({0, 0, 1 }, 1.f);
            planes[FrustumPlane::TOP] = Plane({0, 0, -1}, 1.f );
            planes[FrustumPlane::NEAR] = Plane({0, 1, 0 }, 1.f);
            planes[FrustumPlane::FAR] = Plane({0, -1, 0}, 1.f );
        }

        Frustum(const float4x4 & viewProj)
        {
            planes[FrustumPlane::RIGHT].equation.x = viewProj[0][3] - viewProj[0][0];
            planes[FrustumPlane::RIGHT].equation.y = viewProj[1][3] - viewProj[1][0];
            planes[FrustumPlane::RIGHT].equation.z = viewProj[2][3] - viewProj[2][0];
            planes[FrustumPlane::RIGHT].equation.w = viewProj[3][3] - viewProj[3][0];

            planes[FrustumPlane::LEFT].equation.x = viewProj[0][3] + viewProj[0][0];
            planes[FrustumPlane::LEFT].equation.y = viewProj[1][3] + viewProj[1][0];
            planes[FrustumPlane::LEFT].equation.z = viewProj[2][3] + viewProj[2][0];
            planes[FrustumPlane::LEFT].equation.w = viewProj[3][3] + viewProj[3][0];

            planes[FrustumPlane::BOTTOM].equation.x = viewProj[0][3] + viewProj[0][1];
            planes[FrustumPlane::BOTTOM].equation.y = viewProj[1][3] + viewProj[1][1];
            planes[FrustumPlane::BOTTOM].equation.z = viewProj[2][3] + viewProj[2][1];
            planes[FrustumPlane::BOTTOM].equation.w = viewProj[3][3] + viewProj[3][1];

            planes[FrustumPlane::TOP].equation.x = viewProj[0][3] - viewProj[0][1];
            planes[FrustumPlane::TOP].equation.y = viewProj[1][3] - viewProj[1][1];
            planes[FrustumPlane::TOP].equation.z = viewProj[2][3] - viewProj[2][1];
            planes[FrustumPlane::TOP].equation.w = viewProj[3][3] - viewProj[3][1];

            planes[FrustumPlane::FAR].equation.x = viewProj[0][3] - viewProj[0][2];
            planes[FrustumPlane::FAR].equation.y = viewProj[1][3] - viewProj[1][2];
            planes[FrustumPlane::FAR].equation.z = viewProj[2][3] - viewProj[2][2];
            planes[FrustumPlane::FAR].equation.w = viewProj[3][3] - viewProj[3][2];

            planes[FrustumPlane::NEAR].equation.x = viewProj[0][3] + viewProj[0][2];
            planes[FrustumPlane::NEAR].equation.y = viewProj[1][3] + viewProj[1][2];
            planes[FrustumPlane::NEAR].equation.z = viewProj[2][3] + viewProj[2][2];
            planes[FrustumPlane::NEAR].equation.w = viewProj[3][3] + viewProj[3][2];

            for (auto p : planes) p.normalize();
        }
        
        // Returns F/N/T/B/L/R
        std::array<float3, 6> get_corners() const
        {
            std::array<float3, 6> corners;

            Line farLeft = intersect_plane_plane(planes[FrustumPlane::LEFT], planes[FrustumPlane::FAR]);
            Line farRight = intersect_plane_plane(planes[FrustumPlane::RIGHT], planes[FrustumPlane::FAR]);
            Line nearLeft = intersect_plane_plane(planes[FrustumPlane::LEFT], planes[FrustumPlane::NEAR]);
            Line nearRight = intersect_plane_plane(planes[FrustumPlane::RIGHT], planes[FrustumPlane::NEAR]);

            corners[0] = intersect_line_plane(farLeft, planes[FrustumPlane::TOP]);
            corners[1] = intersect_line_plane(farRight, planes[FrustumPlane::TOP]);
            corners[2] = intersect_line_plane(farLeft, planes[FrustumPlane::BOTTOM]);
            corners[3] = intersect_line_plane(farRight, planes[FrustumPlane::BOTTOM]);
            corners[4] = intersect_line_plane(nearLeft, planes[FrustumPlane::TOP]);
            corners[5] = intersect_line_plane(nearRight, planes[FrustumPlane::TOP]);
            corners[6] = intersect_line_plane(nearLeft, planes[FrustumPlane::BOTTOM]);
            corners[7] = intersect_line_plane(nearRight, planes[FrustumPlane::BOTTOM]);

            return corners;
        }

    };

}

#endif // end geometric_h
