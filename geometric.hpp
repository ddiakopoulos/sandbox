// Inspired by https://github.com/melax/sandbox/blob/master/include/geometric.h - copyright 2014 Stan Melax (MIT license)
// Some code borrowed from https://github.com/bclucas/Alloy-Graphics-Library/blob/master/include/core/AlloyMath.h (public domain)

#ifndef geometric_h
#define geometric_h

#include "linear_algebra.hpp"
#include "math_util.hpp"

namespace math
{

    /////////////////////////
    // Matrix diagonalizer //
    /////////////////////////

    template<class T> vec<T,4> diagonalizer(const mat<T,3,3> &A)
    {
        // A must be a symmetric matrix.
        // returns quaternion q such that its corresponding matrix Q 
        // can be used to Diagonalize A
        // Diagonal matrix D = Q * A * Transpose(Q);  and  A = QT*D*Q
        // The rows of q are the eigenvectors D's diagonal is the eigenvalues
        // As per 'row' convention if double3x3 Q = qmat(q); then v*Q = q*v*conj(q)
        int maxsteps=24;  // certainly wont need that many.
        int i;
        vec<T,4> q(0,0,0,1);
        for(i=0;i<maxsteps;i++)
        {
            auto Q = qmat(q); // v*Q == q*v*conj(q)
            auto D = mul(transpose(Q), mul(A, Q)); // A = Q^T*D*Q
            vec<T,3> offdiag(D(2,1),D(2,0),D(1,0)); // elements not on the diagonal
            vec<T,3> om(std::abs(offdiag.x), std::abs(offdiag.y), std::abs(offdiag.z)); // mag of each offdiag elem
            int k = (om.x > om.y && om.x > om.z) ? 0 : (om.y > om.z) ? 1 : 2; // index of largest element of offdiag
            int k1 = (k+1)%3;
            int k2 = (k+2)%3;
            if(offdiag[k]==0.0f) break;  // diagonal already
            T thet = (D(k2,k2)-D(k1,k1))/(2.0f*offdiag[k]);
            T sgn = (thet>0.0f)?1.0f:-1.0f;
            thet    *= sgn; // make it positive
            T t = sgn /(thet +((thet<1.E6f)?std::sqrt((thet*thet)+1.0f):thet)) ; // sign(T)/(|T|+sqrt(T^2+1))
            T c = 1.0f/std::sqrt(t*t+1.0f); //  c= 1/(t^2+1) , t=s/c 
            if(c==1.0f) break;  // no room for improvement - reached machine precision.
            vec<T,4> jr(0,0,0,0); // jacobi rotation for this iteration.
            jr[k] = sgn*std::sqrt((1.0f-c)/2.0f);  // using 1/2 angle identity sin(a/2) = sqrt((1-cos(a))/2)  
            jr[k] *= -1.0f; // since our quat-to-matrix convention was for v*M instead of M*v
            jr.w  = std::sqrt(1.0f - (jr[k]*jr[k]));
            if(jr.w==1.0f) break; // reached limits of doubleing point precision
            q =  qmul(q,jr);
            q = normalize(q); 
        } 
        auto ev = mul(transpose(qmat(q)), mul(A, qmat(q)));
        if(ev(0,0)>ev(2,2))
            q = qmul(q,normalize(vec<T,4>(0,0.7f,0,0.7f)));
        ev = mul(transpose(qmat(q)), mul(A, qmat(q)));
        if(ev(1,1)>ev(2,2))
            q = qmul(q,normalize(vec<T,4>(0.7f,0,0,0.7f)));
        ev = mul(transpose(qmat(q)), mul(A, qmat(q)));
        if(ev(0,0)>ev(1,1))
            q = qmul(q,normalize(vec<T,4>(0,0,0.7f,0.7f)));
        if(qzdir(q).y<0)
            q = qmul(q,vec<T,4>(1,0,0,0));
        if(qydir(q).x<0)
            q = qmul(q,vec<T,4>(0,0,1,0));
        ev = mul(transpose(qmat(q)), mul(A, qmat(q)));
        return q;
    }

    //////////////////
    // Bounding Box //
    //////////////////

    template<class T, int M> struct box 
    {
        vec<T, M> position;
        vec<T, M> dimensions;

        box() : position((T) 0), dimensions((T) 0)  { }
        box(vec<T, M> pt, vec<T, M> dims) : position(pt), dimensions(dims) { }

        bool contains(const vec<T, M>& qt) const 
        {
            for (int m = 0; m < M; m++)
                if (qt[m] < position[m] || qt[m] >= position[m] + dimensions[m])
                    return false;
            return true;
        }

        void clamp(const box<T, M>& other) 
        {
            dimensions = math::min(dimensions, other.dimensions);
            position = math::clamp(position, other.position, other.position + other.dimensions - dimensions);
        }

        void intersect(const box<T, M>& other) 
        {
            vec<T, M> mn = math::max(position, other.position);
            vec<T, M> mx = math::min(max(), other.max());
            dimensions = mx - mn;
            for (int m = 0; m < M; m++)
                dimensions[m] = std::max(dimensions[m], (T) 0);
            position = mn;
        }

        vec<T, M> max() const 
        {
            return position + dimensions;
        }

        vec<T, M> min() const 
        {
            return position;
        }

        vec<T, M> center() const 
        {
            return position + dimensions / (T) (2);
        }

        vec<T, M> clamp(const vec<T, M>& pt, const box<T, M>& parent) 
        {
            return math::clamp(pt, parent.position, parent.position + parent.dimensions - dimensions);
        }
    };

    ////////////////////////////////////
    // Construct rotation quaternions //
    ////////////////////////////////////

    float4 make_rotation_quat_axis_angle(const float3 & axis, float angle) 
    { 
        return {axis * std::sin(angle/2), std::cos(angle/2)}; 
    }

    float4 make_rotation_quat_around_x(float angle) 
    { 
        return make_rotation_quat_axis_angle({1,0,0}, angle); 
    }

    float4 make_rotation_quat_around_y(float angle) 
    { 
        return make_rotation_quat_axis_angle({0,1,0}, angle); 
    }

    float4 make_rotation_quat_around_z(float angle) 
    { 
        return make_rotation_quat_axis_angle({0,0,1}, angle); 
    }

    float4 make_rotation_quat_between_vectors(const float3 & from, const float3 & to)
    {
        auto a = normalize(from), b = normalize(to);
        return make_rotation_quat_axis_angle(normalize(cross(a,b)), std::acos(dot(a,b)));
    }

    float4 make_rotation_quat_from_rotation_matrix(const float3x3 & m)
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

    float4 make_rotation_quat_from_pose_matrix(const float4x4 & m)
    { 
        return make_rotation_quat_from_rotation_matrix({m.x.xyz(),m.y.xyz(),m.z.xyz()}); 
    }

    // returns unit length q such that qmat(q)^t * matrix * qmat(q) is a diagonal matrix
    float4 make_rotation_quat_to_diagonalize_symmetric_matrix(const float3x3 & matrix) 
    { 
        return diagonalizer(matrix); 
    }

    // returns unit length q such that qmat(q)^t * matrix * qmat(q) is a diagonal matrix
    double4 make_rotation_quat_to_diagonalize_symmetric_matrix(const double3x3 & matrix) 
    { 
        return diagonalizer(matrix); 
    }

    inline float4 make_axis_angle_rotation_quat(const float4& q)
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
            {2*nearZ/(right-left),0,0,0},
            {0,2*nearZ/(top-bottom),0,0},
            {(right+left)/(right-left), (top+bottom)/(top-bottom), -(farZ+nearZ)/(farZ-nearZ), -1},
            {0,0,-2*farZ*nearZ/(farZ-nearZ),0}
        };
    }

    inline float4x4 make_perspective_matrix_rh_hl(float vFovInRadians, float aspectRatio, float nearZ, float farZ)
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
        auto r = mul(transform, float4(coord,1)); return r.xyz()/r.w;
    }

    inline float3 transform_vector(const float4x4 & transform, const float3 & vector) 
    { 
        return mul(transform, float4(vector,0)).xyz(); 
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

    float4x4 make_view_matrix_from_pose(const Pose & pose) 
    { 
        return pose.inverse().matrix(); 
    }

} // end namespace math

#endif // end geometric_h
