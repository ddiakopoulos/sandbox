// See COPYING file for attribution information

#ifndef geometric_h
#define geometric_h

#include "linear_algebra.hpp"
#include "math_util.hpp"
#include <ostream>

namespace avl
{

    //////////////////
    // Bounding Box //
    //////////////////

    template<class T, int M> struct Box
    {
        vec<T, M> position;
        vec<T, M> dimensions;

        Box() : position((T) 0), dimensions((T) 0)  { }
        Box(vec<T, M> pt, vec<T, M> dims) : position(pt), dimensions(dims) { }

        bool contains(const vec<T, M> & qt) const
        {
            for (int m = 0; m < M; m++)
            {
                if (qt[m] < position[m] || qt[m] >= position[m] + dimensions[m])
                {
                    return false;
                }
            }
            return true;
        }
        
        bool intersects(const Box<T, M> & other) const
        {
            vec<T, M> mn = max(min(), other.min());
    		vec<T, M> mx = min(max(), other.max());
    		vec<T, M> dims = mx - mn;
    		int a = 1;
    		for (int m = 0; m < M; m++) 
    		{
    			dims[m] = max(dims[m], (T) 0);
    			a *= dims[m];
    		}
    		return (a > 0);
        }
        
        vec<T, M> min() const { return position; }
        vec<T, M> max() const { return position + dimensions; }
		
        vec<T, M> center() const 
        {
            return position + (0.5f * (dimensions - position));
        }
        
        float volume() const
        {
            return (dimensions.x - position.x) * (dimensions.y - position.y) * (dimensions.z - position.z);
        }
    };
    
    struct Bounds
    {
        float x0;
        float y0;
        float x1;
        float y1;
        
        Bounds() : x0(0), y0(0), x1(0), y1(0) {};
        Bounds(float x0, float y0, float x1, float y1) : x0(x0), y0(y0), x1(x1), y1(y1) {}
        
        bool inside(const float px, const float py) const { return px >= x0 && py >= y0 && px < x1 && py < y1; }
        bool inside(const float2 & point) const { return inside(point.x, point.y); }
        
        float2 get_min() const { return {x0,y0}; }
        float2 get_max() const { return {x1,y1}; }
        
        float2 get_size() const { return get_max() - get_min(); }
        float2 get_center() const { return {get_center_x(), get_center_y()}; }
        
        float get_center_x() const { return (x0+x1)/2; }
        float get_center_y() const { return (y0+y1)/2; }
        
        float width() const { return x1 - x0; }
        float height() const { return y1 - y0; }
    };
    
    //template<class T> std::ostream & operator << (std::ostream & a, Bounds b) { return a << "[" << b.x0 << ", " << b.y0 << ", " << b.x1 << ", " << b.y1 << "]"; }

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
        const float s = 1/sqrt(t) / 2;
        const float4 qp = float4(pre.y * m(2,1) - pre.z * m(1,2), pre.z * m(0,2) - pre.x * m(2,0), pre.x * m(1,0) - pre.y * m(0,1), t) * s;
        return qmul(qp, post);
    }

    inline float4 make_rotation_quat_from_pose_matrix(const float4x4 & m)
    { 
        return make_rotation_quat_from_rotation_matrix({m.x.xyz(),m.y.xyz(),m.z.xyz()}); 
    }

    inline float4 make_axis_angle_rotation_quat(const float4 & q)
    {
        float4 result;
        result.w = 2.0f * (float) acosf(std::max(-1.0f, std::min(q.w, 1.0f))); // angle
        float den = (float) sqrt(std::abs(1.0 - q.w * q.w));
        if (den > 1E-5f)
        {
            result.x = q.x / den;
            result.y = q.y / den;
            result.z = q.z / den;
        }
        else
        {
            result.x = 1.0f;
            result.y = 0.0f;
            result.z = 0.0f;
        }
        return result;
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

    inline float4x4 make_projection_matrix_from_frustrum_rh_gl(float left, float right, float bottom, float top, float nearZ, float farZ)
    {
        return 
        {
            {2 * nearZ / (right-left), 0, 0, 0},
            {0,2 * nearZ / (top-bottom), 0 ,0},
            {(right + left) / (right-left), (top + bottom) / (top - bottom), -(farZ + nearZ) / (farZ - nearZ), -1},
            {0, 0, -2 * farZ * nearZ / (farZ - nearZ), 0}
        };
    }

    inline float4x4 make_perspective_matrix_rh_gl(float vFovInRadians, float aspectRatio, float nearZ, float farZ)
    {
        const float top = nearZ * std::tan(vFovInRadians/2.f), right = top * aspectRatio;
        return make_projection_matrix_from_frustrum_rh_gl(-right, right, -top, top, nearZ, farZ);
    }

    inline float4x4 make_orthographic_perspective_matrix(float l, float r, float b, float t, float n, float f)
    {
        return {{2/(r-l),0,0,0}, {0,2/(t-b),0,0}, {0,0,-2/(f-n),0}, {-(r+l)/(r-l),-(t+b)/(t-b),-(f+n)/(f-n),1}};
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
    
    
    //////////////////////////////////////////
    //     | 1-2Nx^2   -2NxNy  -2NxNz  -2NxD |
    // m = |  -2NxNy  1-2Ny^2  -2NyNz  -2NyD |
    //     |  -2NxNz  -2NyNz  1-2Nz^2  -2NzD |
    //     |    0       0       0       1    |
    //
    // Where (Nx,Ny,Nz,D) are the coefficients of plane equation (xNx + yNy + zNz + D = 0) &
    // (Nx,Ny,Nz) is the normal vector of given plane.
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

    ///////////
    // Poses //
    ///////////

    struct Pose
    {
        float4      orientation;                                    // Orientation of an object, expressed as a rotation quaternion from the base orientation
        float3      position;                                       // Position of an object, expressed as a translation vector from the base position

                    Pose()                                          : Pose({0,0,0,1}, {0,0,0}) {}
                    Pose(const float4 & orientation,
                         const float3 & position)                   : orientation(orientation), position(position) {}
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
    
    inline void look_at_pose(float3 eyePoint, float3 target, Pose & p)
    {
        const float3 worldUp = {0,1,0};
        float3 zDir = normalize(eyePoint - target);
        float3 xDir = normalize(cross(worldUp, zDir));
        float3 yDir = cross(zDir, xDir);
        p.position = eyePoint;
        p.orientation = normalize(make_rotation_quat_from_rotation_matrix({xDir, yDir, zDir}));
    }
    
    inline Pose look_at_pose(float3 eyePoint, float3 target)
    {
        Pose p;
        look_at_pose(eyePoint, target, p);
        return p;
    }
    
    /////////////
    //   Ray   //
    /////////////
    
    class Ray
    {
        bool signX, signY, signZ;
        float3 invDirection;
    public:
        
        float3 origin;
        float3 direction;
        
        Ray() {}
        Ray(const float3 &aOrigin, const float3 &aDirection) : origin(aOrigin) { set_direction(aDirection); }
        
        void set_origin(const float3 &aOrigin) { origin = aOrigin; }
        const float3& get_origin() const { return origin; }
        
        void set_direction(const float3 &aDirection)
        {
            direction = aDirection;
            invDirection = float3(1.0f / direction.x, 1.0f / direction.y, 1.0f / direction.z);
            signX = (direction.x < 0.0f) ? 1 : 0;
            signY = (direction.y < 0.0f) ? 1 : 0;
            signZ = (direction.z < 0.0f) ? 1 : 0;
        }
        
        const float3 & get_direction() const { return direction; }
        const float3 & get_inv_direction() const { return invDirection; }
        
        char getSignX() const { return signX; }
        char getSignY() const { return signY; }
        char getSignZ() const { return signZ; }
        
        void transform(const float4x4 & matrix)
        {
            origin = transform_vector(matrix, origin);
            set_direction(get_rotation_submatrix(matrix) * direction);
        }
        
        Ray transformed(const float4x4 & matrix) const
        {
            Ray result;
            result.origin = transform_vector(matrix, origin);
            result.set_direction(get_rotation_submatrix(matrix) * direction);
            return result;
        }
        
        float3 calculate_position(float t) const { return origin + direction * t; }
    };
    
    inline Ray operator * (const Pose & pose, const Ray & ray)
    {
        return {pose.transform_coord(ray.get_origin()), pose.transform_vector(ray.get_direction())};
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
    
    /////////////
    // Helpers //
    /////////////
    
    inline void find_orthonormals(const float3 normal, float3 & orthonormal1, float3 & orthonormal2)
    {
        const float4x4 OrthoX = make_rotation_matrix({1, 0, 0}, ANVIL_PI / 2);
        const float4x4 OrthoY = make_rotation_matrix({0, 1, 0}, ANVIL_PI / 2);;
        
        float3 w = transform_vector(OrthoX, normal);
        float d = dot(normal, w);
        if (std::abs(d) > 0.6f)
        {
            w = transform_vector(OrthoY, normal);
        }
        
        w = normalize(w);
        
        orthonormal1 = cross(normal, w);
        orthonormal1 = normalize(orthonormal1);
        orthonormal2 = cross(normal, orthonormal1);
        orthonormal2 = normalize(orthonormal2);
    }
    
    inline float find_quaternion_twist(float4 q, float3 axis)
    {
        normalize(axis);
        
        //get the plane the axis is a normal of
        float3 orthonormal1, orthonormal2;
        
        find_orthonormals(axis, orthonormal1, orthonormal2);
        
        float3 transformed = transform_vector(q, orthonormal1); // orthonormal1 * q;
        
        //project transformed vector onto plane
        float3 flattened = transformed - (dot(transformed, axis) * axis);
        flattened = normalize(flattened);
        
        //get angle between original vector and projected transform to get angle around normal
        float a = (float) acos(dot(orthonormal1, flattened));
        
        return a;
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
        Bounds resolve(const Bounds & r) const { return { x0.resolve(r.x0, r.x1), y0.resolve(r.y0, r.y1), x1.resolve(r.x0, r.x1), y1.resolve(r.y0, r.y1) }; }
        bool is_fixed_width() const { return x0.a == x1.a; }
        bool is_fixed_height() const { return y0.a == y1.a; }
        float fixed_width() const { return x1.b - x0.b; }
        float fixed_height() const { return y1.b - y0.b; }
    };

}

#endif // end geometric_h
