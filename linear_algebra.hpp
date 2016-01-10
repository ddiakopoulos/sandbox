// See COPYING file for attribution information

#ifndef linear_algebra_h
#define linear_algebra_h

#include <cmath>
#include <cstdint>
#include <stdint.h>
#include <functional>
#include <istream>
#include <ostream>
#include <tuple>
#include <limits>

namespace avl
{
    
    // Fixed-size, M-element vector
    template<class T, int M> struct vec; 
        
    template<class T> struct vec<T,2>
    {
        T           x,y;

                    vec()                                       : x(), y() {}
                    vec(T x, T y)                               : x(x), y(y) {}
        explicit    vec(T s)                                    : x(s), y(s) {}
        template<class U> 
        explicit    vec(const vec<U,2> & v)                     : x((T)v.x), y((T)v.y) {}
        explicit    vec(const T * p)                            : vec(*reinterpret_cast<const vec *>(p)) {}

        T &         operator [] (int i)                         { return (&x)[i]; }
        const T &   operator [] (int i) const                   { return (&x)[i]; }
        bool        operator == (const vec & r) const           { return x==r.x && y==r.y; }
        bool        operator < (const vec & r) const            { return std::make_tuple(x, y) < std::make_tuple(r.x, r.y); }

        template<class F> vec apply(const vec & r, F f) const   { return {f(x,r.x), f(y,r.y)}; }
        template<class F> vec apply(const T & r, F f) const     { return {f(x,r), f(y,r)}; }
    };

    template<class T> struct vec<T,3>
    {
        T           x,y,z;

                    vec()                                       : x(), y(), z() {}
                    vec(T x, T y, T z)                          : x(x), y(y), z(z){}
                    vec(const vec<T,2> & xy, T z)               : x(xy.x), y(xy.y), z(z) {}
        explicit    vec(T s)                                    : x(s), y(s), z(s) {}
        template<class U> 
        explicit    vec(const vec<U,3> & v)                     : x((T)v.x), y((T)v.y), z((T)v.z) {}
        explicit    vec(const T * p)                            : vec(*reinterpret_cast<const vec *>(p)) {}
        explicit    vec(T * p)                                  : vec(*reinterpret_cast<vec *>(p)) {}
        T &         operator [] (int i)                         { return (&x)[i]; }
        const T &   operator [] (int i) const                   { return (&x)[i]; }
        bool        operator == (const vec & r) const           { return x==r.x && y==r.y && z==r.z; }
        bool        operator < (const vec & r) const            { return std::make_tuple(x,y,z) < std::make_tuple(r.x,r.y,r.z); }
        bool        operator > (const vec & r) const            { return std::make_tuple(x,y,z) > std::make_tuple(r.x,r.y,r.z); }

        vec<T, 2>   xy() const                                  { return vec<T, 2>(x, y); };
        template<class F> vec apply(const vec & r, F f) const   { return {static_cast<float>(f(x,r.x)), static_cast<float>(f(y,r.y)), static_cast<float>(f(z,r.z))}; }
        template<class F> vec apply(const T & r, F f) const     { return {static_cast<float>(f(x,r)), static_cast<float>(f(y,r)), static_cast<float>(f(z,r))}; }
    };

    template<class T> struct vec<T,4>
    {
        T           x,y,z,w;

                    vec()                                       : x(), y(), z(), w() {}
                    vec(T x, T y, T z, T w)                     : x(x), y(y), z(z), w(w) {}
                    vec(const vec<T,2> & xy, T z, T w)          : x(xy.x), y(xy.y), z(z), w(w) {}
                    vec(const vec<T,3> & xyz, T w)              : x(xyz.x), y(xyz.y), z(xyz.z), w(w) {}
        explicit    vec(T s)                                    : x(s), y(s), z(s), w(s) {}
        template<class U> 
        explicit    vec(const vec<U,4> & v)                     : x((T)v.x), y((T)v.y), z((T)v.z), w((T)v.w) {}
        explicit    vec(const T * p)                            : vec(*reinterpret_cast<const vec *>(p)) {}

        T &         operator [] (int i)                         { return (&x)[i]; } // Array-style access
        const T &   operator [] (int i) const                   { return (&x)[i]; } // Array-style access
        bool        operator == (const vec & r) const           { return x==r.x && y==r.y && z==r.z && w==r.w; }
        bool        operator < (const vec & r) const            { return std::make_tuple(x,y,z,w) < std::make_tuple(r.x,r.y,r.z,r.w); }
        vec<T,3>    xyz() const                                 { return {x,y,z}; }
        vec<T,2>    xy() const                                  { return{ x, y }; }

        template<class F> vec apply(const vec & r, F f) const   { return {f(x,r.x), f(y,r.y), f(z,r.z), f(w,r.w)}; }
        template<class F> vec apply(const T & r, F f) const     { return {f(x,r), f(y,r), f(z,r), f(w,r)}; }
    };

    template<class T, int M> bool        operator != (const vec<T, M> & a, const vec<T, M> & b)   { return !(a == b); }

    template<class T, int M> vec<T, M>   operator - (const vec<T, M> & a)                         { return a.apply(T(), [](T a, T) { return -a; }); }
    template<class T, int M> vec<T, M>   operator + (const vec<T, M> & a, const vec<T, M> & b)    { return a.apply(b, [](T a, T b) { return a + b; }); }
    template<class T, int M> vec<T, M>   operator - (const vec<T, M> & a, const vec<T, M> & b)    { return a.apply(b, [](T a, T b) { return a - b; }); }
    template<class T, int M> vec<T, M>   operator * (const vec<T, M> & a, const vec<T, M> & b)    { return a.apply(b, [](T a, T b) { return a * b; }); }
    template<class T, int M> vec<T, M>   operator / (const vec<T, M> & a, const vec<T, M> & b)    { return a.apply(b, [](T a, T b) { return a / b; }); }
    template<class T, int M> vec<T, M>   operator + (const vec<T, M> & a, const T & b)            { return a.apply(b, [](T a, T b) { return a + b; }); }
    template<class T, int M> vec<T, M>   operator - (const vec<T, M> & a, const T & b)            { return a.apply(b, [](T a, T b) { return a - b; }); }
    template<class T, int M> vec<T, M>   operator * (const vec<T, M> & a, const T & b)            { return a.apply(b, [](T a, T b) { return a * b; }); }
    template<class T, int M> vec<T, M>   operator / (const vec<T, M> & a, const T & b)            { return a.apply(b, [](T a, T b) { return a / b; }); }
    
    template<class T, int M> vec<T, M>   operator - (const T & a, const vec<T, M> & b)            { return b - a; }
    template<class T, int M> vec<T, M>   operator + (const T & a, const vec<T, M> & b)            { return b + a; }
    template<class T, int M> vec<T, M>   operator / (const T & a, const vec<T, M> & b)            { return b / a; }
    template<class T, int M> vec<T, M>   operator * (const T & a, const vec<T, M> & b)            { return b * a; }
    
    template<class T, int M> vec<T, M> & operator += (vec<T, M> & a, const vec<T, M> & b)         { return a = a + b; }
    template<class T, int M> vec<T, M> & operator -= (vec<T, M> & a, const vec<T, M> & b)         { return a = a - b; }
    template<class T, int M> vec<T, M> & operator *= (vec<T, M> & a, const vec<T, M> & b)         { return a = a * b; }
    template<class T, int M> vec<T, M> & operator /= (vec<T, M> & a, const vec<T, M> & b)         { return a = a / b; }
    template<class T, int M> vec<T, M> & operator += (vec<T, M> & a, const T & b)                 { return a = a + b; }
    template<class T, int M> vec<T, M> & operator -= (vec<T, M> & a, const T & b)                 { return a = a - b; }
    template<class T, int M> vec<T, M> & operator *= (vec<T, M> & a, const T & b)                 { return a = a * b; }
    template<class T, int M> vec<T, M> & operator /= (vec<T, M> & a, const T & b)                 { return a = a / b; }

    //////////////////////
    // Vector functions //
    //////////////////////

    template<class T, int N> vec<T, N>   clamp(const vec<T, N> & a, const vec<T, N> & mn, const vec<T, N> & mx)            { return max(min(a,mx),mn); }
    template<class T> T                  cross(const vec<T, 2> & a, const vec<T, 2> & b)          { return a.x*b.y - a.y*b.x; }
    template<class T> vec<T, 3>          cross(const vec<T, 3> & a, const vec<T, 3> & b)          { return {a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x}; }
    template<class T, int M> T           distance(const vec<T, M> & a, const vec<T, M> & b)       { return length(b-a); }
    template<class T, int M> T           distanceSqr(const vec<T, M> & a, const vec<T, M> & b)    { return lengthSqr(b-a); }
    template<class T> T                  dot(const vec<T, 2> & a, const vec<T, 2> & b)            { return a.x*b.x + a.y*b.y; }
    template<class T> T                  dot(const vec<T, 3> & a, const vec<T, 3> & b)            { return a.x*b.x + a.y*b.y + a.z*b.z; }
    template<class T> T                  dot(const vec<T, 4> & a, const vec<T, 4> & b)            { return a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w; }
    template<class T, int M> T           lengthSqr(const vec<T, M> & a)                           { return dot(a, a); }
    template<class T, int M> T           sum(const vec<T, M> & a)                                 { T s = a[0]; for(int m=1; m<M; ++m) s += a[m]; return s; }
    template<class T, int M> T           lengthL1(const vec<T, M> & a)                            { T s = a[0]; for(int m=1; m<M; ++m) s += abs(a[m]); return s; }
    template<class T, int M> T           lengthL2(const vec<T, M> & a)                            { return std::sqrt(lengthSqr(a)); }
    template<class T, int M> T           length(const vec<T, M> & a)                              { return lengthL2(a); }
    template<class T, int M> vec<T, M>   lerp(const vec<T, M> & a, const vec<T, M> & b, T t)      { return a*(1-t) + b*t; }
    template<class T, int M> T           max(const vec<T,M> & a)                                  { T s = a[0]; for(int i=1; i<M; ++i) s = std::max(s, a[i]); return s; }
    template<class T, int M> vec<T, M>   max(const vec<T, M> & a, const vec<T, M> & b)            { return a.apply(b, [](T a, T b) { return std::max(a, b); }); }
    template<class T, int M> T           min(const vec<T,M> & a)                                  { T s = a[0]; for(int i=1; i<M; ++i) s = std::min(s, a[i]); return s; }
    template<class T, int M> vec<T, M>   min(const vec<T, M> & a, const vec<T, M> & b)            { return a.apply(b, [](T a, T b) { return std::min(a, b); }); }
    template<class T, int M> vec<T, M>   nlerp(const vec<T, M> & a, const vec<T, M> & b, T t)     { return normalize(lerp(a, b, t)); }
    template<class T, int M> vec<T, M>   normalize(const vec<T, M> & a)                           { return a / std::max(T(1E-6), length(a)); }
    template<class T, int M> vec<T, M>   vround(const vec<T, M> & a)                               { return a.apply(T(), [](T a, T) { return ::round(a); }); }
    template<class T, int M> vec<T, M>   vfloor(const vec<T, M> & a)                              { return a.apply(T(), [](T a, T) { return ::floor(a); }); }
    template<class T, int M> vec<T, M>   vceil(const vec<T, M> & a)                               { return a.apply(T(), [](T a, T) { return ::ceil(a); }); }
    template<class T, int M> vec<T, M>   vabs(const vec<T, M> & a)                                { return a.apply(T(), [](T a, T) { return ::abs(a); }); }
    template<class T, int M> vec<T, M>   pow(const vec<T, M> & a, const vec<T, M> & b)            { return a.apply(b, [](T a, T b) { return ::pow(a, b); }); }
    template<class T, int M> vec<T, M>   exp(const vec<T, M> & a)                                 { return a.apply(T(), [](T a, T) { return ::exp(a); }); }
    
    template<class T> vec<T,4>           qconj(const vec<T,4> & q)                                { return {-q.x,-q.y,-q.z,q.w}; }
    template<class T> vec<T,4>           qinv(const vec<T,4> & q)                                 { return qconj(q)/lengthSqr(q); }
    template<class T> vec<T,4>           qmul(const vec<T,4> & a, const vec<T,4> & b)             { return {a.x*b.w+a.w*b.x+a.y*b.z-a.z*b.y, a.y*b.w+a.w*b.y+a.z*b.x-a.x*b.z, a.z*b.w+a.w*b.z+a.x*b.y-a.y*b.x, a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z}; }
    
    // Construct quaternion to describe rotation in radians around an arbitrary axis
    template<class T> vec<T,3>           qrot(const vec<T,4> & q, const vec<T,3> & v)             { return qxdir(q)*v.x + qydir(q)*v.y + qzdir(q)*v.z; } // qvq*    
    template<class T> vec<T,3>           qxdir(const vec<T,4> & q)                                { return {1-2*(q.y*q.y+q.z*q.z), 2*(q.x*q.y+q.w*q.z), 2*(q.x*q.z-q.w*q.y)}; }
    template<class T> vec<T,3>           qydir(const vec<T,4> & q)                                { return {2*(q.x*q.y-q.w*q.z), 1-2*(q.x*q.x+q.z*q.z), 2*(q.y*q.z+q.w*q.x)}; }
    template<class T> vec<T,3>           qzdir(const vec<T,4> & q)                                { return {2*(q.x*q.z+q.w*q.y), 2*(q.y*q.z-q.w*q.x), 1-2*(q.x*q.x+q.y*q.y)}; }
    template<class T> vec<T,4>           qlerp(const vec<T,4> & _a, const vec<T,4> & b, T t)       
    { 
        auto a = _a;
        if(dot(a,b) < 0) a = -a;
        float d = dot(a,b);
        if(d >= 1) return a;
        float theta = acosf(d);
        if(theta == 0) return a;
        return a*(sinf(theta-t*theta)/sinf(theta)) + b*(sinf(t*theta)/sinf(theta));
    }

    // Fixed-size, column major matrices
    template<class T, int M, int N> struct mat;

    template<class T, int M> struct mat<T,M,2>
    {
        typedef vec<T,M>        U;
        U                       x, y;
                                mat()                               : x(), y() {}
                                mat(U x, U y)                       : x(x), y(y) {}
        explicit                mat(T s)                            : x(s), y(s) {}
        explicit                mat(const T * p)                    : mat(*reinterpret_cast<const mat *>(p)) {}
        T &                     operator () (int i, int j)          { return (&x)[j][i]; }              // M(i,j) retrieves the i'th row of the j'th column
        const T &               operator () (int i, int j) const    { return (&x)[j][i]; }              // M(i,j) retrieves the i'th row of the j'th column
        const U &               getCol(int j) const                 { return (&x)[j]; }                 // getCol(j) retrieves the j'th column
        vec<T,2>                getRow(int i) const                 { return {x[i],y[i]}; }             // getRow(i) retrieves the i'th row
        void                    setCol(int j, const U & v)          { (&x)[j] = v; }                    // setCol(j) sets the j'th column from a vector
        void                    setRow(int i, const vec<T,2> & v)   { x[i]=v.x; y[i]=v.y; }             // setRow(i) sets the i'th row from a vector
        template<class F> mat   apply(const mat & r, F f) const     { return {x.apply(r.x,f), y.apply(r.y,f)}; }
        template<class F> mat   apply(T r, F f) const               { return {x.apply(r,f), y.apply(r,f)}; }
    };

    template<class T, int M> struct mat<T,M,3>
    {
        typedef vec<T,M>        U;
        U                       x, y, z;
                                mat()                               : x(), y(), z() {}
                                mat(U x, U y, U z)                  : x(x), y(y), z(z) {}
        explicit                mat(T s)                            : x(s), y(s), z(s) {}
        explicit                mat(const T * p)                    : mat(*reinterpret_cast<const mat *>(p)) {}
        T &                     operator () (int i, int j)          { return (&x)[j][i]; }              // M(i,j) retrieves the i'th row of the j'th column
        const T &               operator () (int i, int j) const    { return (&x)[j][i]; }              // M(i,j) retrieves the i'th row of the j'th column
        const U &               getCol(int j) const                 { return (&x)[j]; }                 // getCol(j) retrieves the j'th column
        vec<T,3>                getRow(int i) const                 { return {x[i],y[i],z[i]}; }        // getRow(i) retrieves the i'th row
        void                    setCol(int j, const U & v)          { (&x)[j] = v; }                    // setCol(j) sets the j'th column from a vector
        void                    setRow(int i, const vec<T,3> & v)   { x[i]=v.x; y[i]=v.y; z[i]=v.z; }   // setRow(i) sets the i'th row from a vector
        template<class F> mat   apply(const mat & r, F f) const     { return {x.apply(r.x,f), y.apply(r.y,f), z.apply(r.z,f)}; }
        template<class F> mat   apply(T r, F f) const               { return {x.apply(r,f), y.apply(r,f), z.apply(r,f)}; }
    };

    template<class T, int M> struct mat<T,M,4>
    {
        typedef vec<T,M>        U;
        U                       x, y, z, w;
                                mat()                               : x(), y(), z(), w() {}
                                mat(U x, U y, U z, U w)             : x(x), y(y), z(z), w(w) {}
        explicit                mat(T s)                            : x(s), y(s), z(s), w(s) {}
        explicit                mat(const T * p)                    : mat(*reinterpret_cast<const mat *>(p)) {}
        T &                     operator () (int i, int j)          { return (&x)[j][i]; }                      // M(i,j) retrieves the i'th row of the j'th column
        const T &               operator () (int i, int j) const    { return (&x)[j][i]; }                      // M(i,j) retrieves the i'th row of the j'th column
        const U &               getCol(int j) const                 { return (&x)[j]; }                         // getCol(j) retrieves the j'th column
        vec<T,4>                getRow(int i) const                 { return {x[i],y[i],z[i],w[i]}; }           // getRow(i) retrieves the i'th row
        void                    setCol(int j, const U & v)          { (&x)[j] = v; }                            // setCol(j) sets the j'th column from a vector
        void                    setRow(int i, const vec<T,4> & v)   { x[i]=v.x; y[i]=v.y; z[i]=v.z; w[i]=v.w; } // setRow(i) sets the i'th row from a vector
        template<class F> mat   apply(const mat & r, F f) const     { return {x.apply(r.x,f), y.apply(r.y,f), z.apply(r.z,f), w.apply(r.w,f)}; }
        template<class F> mat   apply(T r, F f) const               { return {x.apply(r,f), y.apply(r,f), z.apply(r,f), w.apply(r,f)}; }
    };

    template<class T, int M, int N> mat<T,M,N> operator - (const mat<T,M,N> & a)                       { return a.apply(T(), [](T x, T) { return -x; }); }
    template<class T, int M, int N> mat<T,M,N> operator + (const mat<T,M,N> & a, const mat<T,M,N> & b) { return a.apply(b, [](T a, T b) { return a+b; }); }
    template<class T, int M, int N> mat<T,M,N> operator - (const mat<T,M,N> & a, const mat<T,M,N> & b) { return a.apply(b, [](T a, T b) { return a-b; }); }
    template<class T, int M, int N> mat<T,M,N> operator * (const mat<T,M,N> & a, T b)                  { return a.apply(b, [](T a, T b) { return a*b; }); }
    template<class T, int M, int N> mat<T,M,N> operator / (const mat<T,M,N> & a, T b)                  { return a.apply(b, [](T a, T b) { return a/b; }); }
    template<class T, int M, int N> mat<T,M,N> operator * (T a, const mat<T,M,N> & b)                  { return b*a; }
    template<class T, int M, int N> mat<T,M,N> & operator += (mat<T,M,N> & a, const mat<T,M,N> & b)    { return a=a+b; }
    template<class T, int M, int N> mat<T,M,N> & operator -= (mat<T,M,N> & a, const mat<T,M,N> & b)    { return a=a-b; }
    template<class T, int M, int N> mat<T,M,N> & operator *= (mat<T,M,N> & a, T b)                     { return a=a*b; }
    template<class T, int M, int N> mat<T,M,N> & operator /= (mat<T,M,N> & a, T b)                     { return a=a/b; }

    template<class T, int M, int N>        vec<T,M>     operator * (const mat<T,M,N> & a, const vec<T,N> & b)   { return mul(a,b); } // Interpret b as column vector (N x 1 matrix)
    template<class T, int M, int N>        vec<T,N>     operator * (const vec<T,M> & a, const mat<T,M,N> & b)   { return mul(a,b); } // Interpret a as row vector (1 x M matrix)
    template<class T, int M, int N, int P> mat<T,M,P>   operator * (const mat<T,M,N> & a, const mat<T,N,P> & b) { return mul(a,b); }
    template<class T, int M>               mat<T,M,M> & operator *= (mat<T,M,M> & a, const mat<T,M,M> & b)      { return a=a*b; }

    //////////////////////
    // Matrix functions //
    //////////////////////

    template<class T, int M, int N> int        cols     (const mat<T,M,N> & a)                         { return N; }
    template<class T, int M, int N> int        rows     (const mat<T,M,N> & a)                         { return M; }
    template<class T, int M, int N> int        size     (const mat<T,M,N> & a)                         { return M*N; }
    template<class T, int M, int N> const T *  data     (const mat<T,M,N> & a)                         { return &a(0,0); }
    template<class T, int M, int N> T *        data     (mat<T,M,N> & a)                               { return &a(0,0); }

    template<class T>               mat<T,2,2> adj      (const mat<T,2,2> & a)                         { return {{a.y.y, -a.x.y}, {-a.y.x, a.x.x}}; }
    template<class T>               mat<T,3,3> adj      (const mat<T,3,3> & a);                        // Definition deferred due to size
    template<class T>               mat<T,4,4> adj      (const mat<T,4,4> & a);                        // Definition deferred due to size
    template<class T>               T          det      (const mat<T,2,2> & a)                         { return a.x.x*a.y.y - a.x.y*a.y.x; }
    template<class T>               T          det      (const mat<T,3,3> & a)                         { return a.x.x*(a.y.y*a.z.z - a.z.y*a.y.z) + a.x.y*(a.y.z*a.z.x - a.z.z*a.y.x) + a.x.z*(a.y.x*a.z.y - a.z.x*a.y.y); }
    template<class T>               T          det      (const mat<T,4,4> & a);                        // Definition deferred due to size
    template<class T, int N>        mat<T,N,N> inv      (const mat<T,N,N> & a)                         { return adj(a)/det(a); }
    template<class T, int M>        vec<T,M>   mul      (const mat<T,M,2> & a, const vec<T,2> & b)     { return a.x*b.x + a.y*b.y; }
    template<class T, int M>        vec<T,M>   mul      (const mat<T,M,3> & a, const vec<T,3> & b)     { return a.x*b.x + a.y*b.y + a.z*b.z; }
    template<class T, int M>        vec<T,M>   mul      (const mat<T,M,4> & a, const vec<T,4> & b)     { return a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w; }
    template<class T, int M, int N> mat<T,M,2> mul      (const mat<T,M,N> & a, const mat<T,N,2> & b)   { return {mul(a,b.x), mul(a,b.y)}; }
    template<class T, int M, int N> mat<T,M,3> mul      (const mat<T,M,N> & a, const mat<T,N,3> & b)   { return {mul(a,b.x), mul(a,b.y), mul(a,b.z)}; }
    template<class T, int M, int N> mat<T,M,4> mul      (const mat<T,M,N> & a, const mat<T,N,4> & b)   { return {mul(a,b.x), mul(a,b.y), mul(a,b.z), mul(a,b.w)}; }
    template<class T, int M>        vec<T,2>   mul      (const vec<T,M> & a, const mat<T,M,2> & b)     { return {dot(a,b.x), dot(a,b.y)}; }
    template<class T, int M>        vec<T,3>   mul      (const vec<T,M> & a, const mat<T,M,3> & b)     { return {dot(a,b.x), dot(a,b.y), dot(a,b.z)}; }
    template<class T, int M>        vec<T,4>   mul      (const vec<T,M> & a, const mat<T,M,4> & b)     { return {dot(a,b.x), dot(a,b.y), dot(a,b.z), dot(a,b.w)}; }
    template<class T, int M>        mat<T,M,2> transpose(const mat<T,2,M> & a)                         { return {a.getRow(0), a.getRow(1)}; }
    template<class T, int M>        mat<T,M,3> transpose(const mat<T,3,M> & a)                         { return {a.getRow(0), a.getRow(1), a.getRow(2)}; }
    template<class T, int M>        mat<T,M,4> transpose(const mat<T,4,M> & a)                         { return {a.getRow(0), a.getRow(1), a.getRow(2), a.getRow(3)}; }
    template<class T>               mat<T,3,3> qmat     (const vec<T,4> & a)                           { return {qxdir(a), qydir(a), qzdir(a)}; }

    ////////////////////////////////////////////////////////////////
    // Definitions of functions which do not fit on a single line //
    ////////////////////////////////////////////////////////////////

    template<class T> mat<T,3,3> adj(const mat<T,3,3> & a) { return { 
        {a.y.y*a.z.z - a.z.y*a.y.z, a.z.y*a.x.z - a.x.y*a.z.z, a.x.y*a.y.z - a.y.y*a.x.z},
        {a.y.z*a.z.x - a.z.z*a.y.x, a.z.z*a.x.x - a.x.z*a.z.x, a.x.z*a.y.x - a.y.z*a.x.x},
        {a.y.x*a.z.y - a.z.x*a.y.y, a.z.x*a.x.y - a.x.x*a.z.y, a.x.x*a.y.y - a.y.x*a.x.y}};
    }

    template<class T> mat<T,4,4> adj(const mat<T,4,4> & a) { return { 
        {a.y.y*a.z.z*a.w.w + a.w.y*a.y.z*a.z.w + a.z.y*a.w.z*a.y.w - a.y.y*a.w.z*a.z.w - a.z.y*a.y.z*a.w.w - a.w.y*a.z.z*a.y.w,
         a.x.y*a.w.z*a.z.w + a.z.y*a.x.z*a.w.w + a.w.y*a.z.z*a.x.w - a.w.y*a.x.z*a.z.w - a.z.y*a.w.z*a.x.w - a.x.y*a.z.z*a.w.w,
         a.x.y*a.y.z*a.w.w + a.w.y*a.x.z*a.y.w + a.y.y*a.w.z*a.x.w - a.x.y*a.w.z*a.y.w - a.y.y*a.x.z*a.w.w - a.w.y*a.y.z*a.x.w,
         a.x.y*a.z.z*a.y.w + a.y.y*a.x.z*a.z.w + a.z.y*a.y.z*a.x.w - a.x.y*a.y.z*a.z.w - a.z.y*a.x.z*a.y.w - a.y.y*a.z.z*a.x.w},
        {a.y.z*a.w.w*a.z.x + a.z.z*a.y.w*a.w.x + a.w.z*a.z.w*a.y.x - a.y.z*a.z.w*a.w.x - a.w.z*a.y.w*a.z.x - a.z.z*a.w.w*a.y.x,
         a.x.z*a.z.w*a.w.x + a.w.z*a.x.w*a.z.x + a.z.z*a.w.w*a.x.x - a.x.z*a.w.w*a.z.x - a.z.z*a.x.w*a.w.x - a.w.z*a.z.w*a.x.x,
         a.x.z*a.w.w*a.y.x + a.y.z*a.x.w*a.w.x + a.w.z*a.y.w*a.x.x - a.x.z*a.y.w*a.w.x - a.w.z*a.x.w*a.y.x - a.y.z*a.w.w*a.x.x,
         a.x.z*a.y.w*a.z.x + a.z.z*a.x.w*a.y.x + a.y.z*a.z.w*a.x.x - a.x.z*a.z.w*a.y.x - a.y.z*a.x.w*a.z.x - a.z.z*a.y.w*a.x.x},
        {a.y.w*a.z.x*a.w.y + a.w.w*a.y.x*a.z.y + a.z.w*a.w.x*a.y.y - a.y.w*a.w.x*a.z.y - a.z.w*a.y.x*a.w.y - a.w.w*a.z.x*a.y.y,
         a.x.w*a.w.x*a.z.y + a.z.w*a.x.x*a.w.y + a.w.w*a.z.x*a.x.y - a.x.w*a.z.x*a.w.y - a.w.w*a.x.x*a.z.y - a.z.w*a.w.x*a.x.y,
         a.x.w*a.y.x*a.w.y + a.w.w*a.x.x*a.y.y + a.y.w*a.w.x*a.x.y - a.x.w*a.w.x*a.y.y - a.y.w*a.x.x*a.w.y - a.w.w*a.y.x*a.x.y,
         a.x.w*a.z.x*a.y.y + a.y.w*a.x.x*a.z.y + a.z.w*a.y.x*a.x.y - a.x.w*a.y.x*a.z.y - a.z.w*a.x.x*a.y.y - a.y.w*a.z.x*a.x.y},
        {a.y.x*a.w.y*a.z.z + a.z.x*a.y.y*a.w.z + a.w.x*a.z.y*a.y.z - a.y.x*a.z.y*a.w.z - a.w.x*a.y.y*a.z.z - a.z.x*a.w.y*a.y.z,
         a.x.x*a.z.y*a.w.z + a.w.x*a.x.y*a.z.z + a.z.x*a.w.y*a.x.z - a.x.x*a.w.y*a.z.z - a.z.x*a.x.y*a.w.z - a.w.x*a.z.y*a.x.z,
         a.x.x*a.w.y*a.y.z + a.y.x*a.x.y*a.w.z + a.w.x*a.y.y*a.x.z - a.x.x*a.y.y*a.w.z - a.w.x*a.x.y*a.y.z - a.y.x*a.w.y*a.x.z,
         a.x.x*a.y.y*a.z.z + a.z.x*a.x.y*a.y.z + a.y.x*a.z.y*a.x.z - a.x.x*a.z.y*a.y.z - a.y.x*a.x.y*a.z.z - a.z.x*a.y.y*a.x.z}};
    }

    template<class T> T det(const mat<T,4,4> & a) { return 
        a.x.x*(a.y.y*a.z.z*a.w.w + a.w.y*a.y.z*a.z.w + a.z.y*a.w.z*a.y.w - a.y.y*a.w.z*a.z.w - a.z.y*a.y.z*a.w.w - a.w.y*a.z.z*a.y.w) +
        a.x.y*(a.y.z*a.w.w*a.z.x + a.z.z*a.y.w*a.w.x + a.w.z*a.z.w*a.y.x - a.y.z*a.z.w*a.w.x - a.w.z*a.y.w*a.z.x - a.z.z*a.w.w*a.y.x) +
        a.x.z*(a.y.w*a.z.x*a.w.y + a.w.w*a.y.x*a.z.y + a.z.w*a.w.x*a.y.y - a.y.w*a.w.x*a.z.y - a.z.w*a.y.x*a.w.y - a.w.w*a.z.x*a.y.y) +
        a.x.w*(a.y.x*a.w.y*a.z.z + a.z.x*a.y.y*a.w.z + a.w.x*a.z.y*a.y.z - a.y.x*a.z.y*a.w.z - a.w.x*a.y.y*a.z.z - a.z.x*a.w.y*a.y.z);
    }

    ///////////////////
    // Stream output //
    ///////////////////

    template<class T> std::ostream & operator << (std::ostream & a, const vec<T,2> & b) { return a << '[' << b.x << ' ' << b.y << ']'; }
    template<class T> std::ostream & operator << (std::ostream & a, const vec<T,3> & b) { return a << '[' << b.x << ' ' << b.y << ' ' << b.z << ']'; }
    template<class T> std::ostream & operator << (std::ostream & a, const vec<T,4> & b) { return a << '[' << b.x << ' ' << b.y << ' ' << b.z << ' ' << b.w << ']'; }

    template<class T, int N> std::ostream & operator << (std::ostream & a, const mat<T,2,N> & b) { return a << '\n' << b.getRow(0) << '\n' << b.getRow(1) << '\n'; }
    template<class T, int N> std::ostream & operator << (std::ostream & a, const mat<T,3,N> & b) { return a << '\n' << b.getRow(0) << '\n' << b.getRow(1) << '\n' << b.getRow(2) << '\n'; }
    template<class T, int N> std::ostream & operator << (std::ostream & a, const mat<T,4,N> & b) { return a << '\n' << b.getRow(0) << '\n' << b.getRow(1) << '\n' << b.getRow(2) << '\n' << b.getRow(3) << '\n'; }

    /////////////////////////////////////
    // Definitions for GLSL-like types //
    /////////////////////////////////////

    typedef vec<int32_t, 2> int2; typedef vec<uint32_t, 2> uint2; typedef vec<uint16_t, 2> ushort2; typedef vec<uint8_t, 2> byte2;
    typedef vec<int32_t, 3> int3; typedef vec<uint32_t, 3> uint3; typedef vec<uint16_t, 3> ushort3; typedef vec<uint8_t, 3> byte3;
    typedef vec<int32_t, 4> int4; typedef vec<uint32_t, 4> uint4; typedef vec<uint16_t, 4> ushort4; typedef vec<uint8_t, 4> byte4;
    typedef vec<float, 2> float2; typedef mat<float, 2, 2> float2x2; typedef mat<float, 2, 3> float2x3; typedef mat<float, 2, 4> float2x4;
    typedef vec<float, 3> float3; typedef mat<float, 3, 2> float3x2; typedef mat<float, 3, 3> float3x3; typedef mat<float, 3, 4> float3x4;
    typedef vec<float, 4> float4; typedef mat<float, 4, 2> float4x2; typedef mat<float, 4, 3> float4x3; typedef mat<float, 4, 4> float4x4;
    typedef vec<double, 2> double2; typedef mat<double, 2, 2> double2x2; typedef mat<double, 2, 3> double2x3; typedef mat<double, 2, 4> double2x4;
    typedef vec<double, 3> double3; typedef mat<double, 3, 2> double3x2; typedef mat<double, 3, 3> double3x3; typedef mat<double, 3, 4> double3x4;
    typedef vec<double, 4> double4; typedef mat<double, 4, 2> double4x2; typedef mat<double, 4, 3> double4x3; typedef mat<double, 4, 4> double4x4;

}

#endif // end linear_algebra_h