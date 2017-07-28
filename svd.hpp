#pragma once

#include <algorithm>
#include <assert.h>
#include "linalg_util.hpp"
#include <vector>

using namespace avl;

template <typename T>
class Matrix
{
    std::vector<T> matrix; // Column Major
    size_t columns;
public:
    Matrix(size_t row, size_t col) : columns(col), matrix(col * row) { }
    T & operator()(size_t x, size_t y)
    {
        return matrix[y * columns + x];
    }
    T * data() { return matrix.data(); }
};

inline float3x3 to_linalg(Matrix<float> & m)
{
    float3x3 result;
    std::memcpy(&result, m.data(), sizeof(float3x3));
    return result;
}

namespace svd
{
    template <typename T>
    inline T sqr(T a)
    {
        return (a == 0 ? 0 : a * a);
    }

    // Computes (a^2 + b^2)^(1/2) without destructive underflow or overflow.
    template <typename T>
    inline T pythagora(T a, T b)
    {
        T abs_a = std::abs(a);
        T abs_b = std::abs(b);
        if (abs_a > abs_b) return abs_a*std::sqrt((T)1.0 + svd::sqr(abs_b / abs_a));
        else return (abs_b == (T)0.0 ? (T)0.0 : abs_b*std::sqrt((T)1.0 + svd::sqr(abs_a / abs_b)));
    };

    template <typename T>
    inline T sign(T a, T b)
    {
        return (b >= 0.0 ? std::abs(a) : -std::abs(a));
    };
}

template <typename T>
inline bool singular_value_decomposition(Matrix<T> & A, int m, int n, std::vector<T> & W, Matrix<T> & V, const int max_iters = 32)
{
    uint32_t flag, i, its, j, jj, k, l, nm;
    T anorm, c, f, g, h, s, scale, x, y, z;
    bool convergence = true;
    g = scale = anorm = 0;

    std::vector<T> rv1(n);

    // Householder reduction to bidiagonal form
    for (i = 0; i < n; i++)
    {
        l = i + 1;
        rv1[i] = scale*g;
        g = s = scale = 0.0;

        if (i < m)
        {
            for (k = i; k<m; k++)
                scale += std::abs(A(k,i));

            if (scale)
            {
                for (k = i; k<m; k++)
                {
                    A(k,i) /= scale;
                    s += A(k,i) * A(k,i);
                }
                f = A(i,i);
                g = -svd::sign<T>(std::sqrt(s), f);
                h = f*g - s;
                A(i,i) = f - g;
                for (j = l; j<n; j++)
                {
                    for (s = 0.0, k = i; k<m; k++)
                        s += A(k,i) * A(k,j);
                    f = s / h;
                    for (k = i; k<m; k++)
                        A(k,j) += f*A(k,i);
                }
                for (k = i; k<m; k++)
                    A(k,i) *= scale;
            }
        }
        W[i] = scale *g;
        g = s = scale = 0.0;
        if (i < m && i != (n - 1))
        {
            for (k = l; k<n; k++)
                scale += std::abs(A(i,k));
            if (scale)
            {
                for (k = l; k<n; k++)
                {
                    A(i,k) /= scale;
                    s += A(i,k) * A(i,k);
                }

                f = A(i,l);
                g = -svd::sign<T>(std::sqrt(s), f);
                h = f*g - s;
                A(i,l) = f - g;

                for (k = l; k<n; k++)
                    rv1[k] = A(i,k) / h;

                for (j = l; j<m; j++)
                {
                    for (s = 0.0, k = l; k<n; k++)
                        s += A(j,k) * A(i,k);
                    for (k = l; k<n; k++)
                        A(j,k) += s*rv1[k];
                }

                for (k = l; k<n; k++)
                    A(i,k) *= scale;
            }
        }
        anorm = std::max(anorm, (std::abs(W[i]) + std::abs(rv1[i])));
    }
    // Accumulation of right-hand transformations.
    for (i = (n - 1); i >= 0; i--)
    {
        //Accumulation of right-hand transformations.
        if (i < (n - 1))
        {
            if (g)
            {
                //Double division to avoid possible underflow.
                for (j = l; j<n; j++)
                    V(j,i) = (A(i,j) / A(i,l)) / g;

                for (j = l; j<n; j++)
                {
                    for (s = 0.0, k = l; k<n; k++)
                        s += A(i,k) * V(k,j);
                    for (k = l; k<n; k++)
                        V(k,j) += s*V(k,i);
                }
            }

            for (j = l; j<n; j++)
                V(i,j) = V(j,i) = 0.0;
        }

        V(i,i) = 1.0;

        g = rv1[i];
        l = i;
    }
    // Accumulation of left-hand transformations.
    for (i = std::min(m, n) - 1; i >= 0; i--)
    {
        l = i + 1;
        g = W[i];

        for (j = l; j<n; j++)
            A(i,j) = 0.0;

        if (g)
        {
            g = (T)1.0 / g;

            for (j = l; j<n; j++)
            {
                for (s = 0.0, k = l; k<m; k++)
                    s += A(k,i) * A(k,j);

                f = (s / A(i,i))*g;

                for (k = i; k<m; k++)
                    A(k,j) += f*A(k,i);
            }
            for (j = i; j<m; j++)
                A(j,i) *= g;
        }
        else
        {
            for (j = i; j < m; j++)
                A(j, i) = 0.0;
        }
        ++A(i,i);
    }

    // Diagonalization of the bidiagonal form: Loop over singular values, and over allowed iterations.
    for (k = (n - 1); k >= 0; k--)
    {
        for (its = 1; its <= max_iters; its++)
        {
            flag = 1;
            for (l = k; l >= 0; l--)
            {
                // Test for splitting.
                nm = l - 1;

                // Note that rv1[1] is always zero.
                if ((T)(std::abs(rv1[l]) + anorm) == anorm)
                {
                    flag = 0;
                    break;
                }
                if ((T)(std::abs(W[nm]) + anorm) == anorm)
                    break;
            }
            if (flag)
            {
                //Cancellation of rv1[l], if l > 1.
                c = 0.0; 
                s = 1.0;
                for (i = l; i <= k; i++)
                {
                    f = s*rv1[i];
                    rv1[i] = c*rv1[i];
                    if ((T)(std::abs(f) + anorm) == anorm)
                        break;

                    g = W[i];
                    h = svd::pythagora<T>(f, g);
                    W[i] = h;
                    h = (T)1.0 / h;
                    c = g*h;
                    s = -f*h;

                    for (j = 0; j<m; j++)
                    {
                        y = A(j,nm);
                        z = A(j,i);
                        A(j,nm) = y*c + z*s;
                        A(j,i) = z*c - y*s;
                    }
                }
            }
            z = W[k];

            // Convergence
            if (l == k) 
            {
                // Singular value is made nonnegative
                if (z < 0.0) 
                {
                    W[k] = -z;
                    for (j = 0; j<n; j++)
                        V(j,k) = -V(j,k);
                }
                break;
            }

            if (its == max_iters) convergence = false;

            x = W[l]; // Shift from bottom 2-by-2 minor.
            nm = k - 1;
            y = W[nm];
            g = rv1[nm];
            h = rv1[k];
            f = ((y - z)*(y + z) + (g - h)*(g + h)) / ((T)2.0*h*y);
            g = svd::pythagora<T>(f, 1.0);
            f = ((x - z)*(x + z) + h*((y / (f + sign(g, f))) - h)) / x;
            c = s = 1.0;

            // Next QR transformation:
            for (j = l; j <= nm; j++)
            {
                i = j + 1;
                g = rv1[i];
                y = W[i];
                h = s*g;
                g = c*g;
                z = svd::pythagora<T>(f, h);
                rv1[j] = z;
                c = f / z;
                s = h / z;
                f = x*c + g*s;
                g = g*c - x*s;
                h = y*s;
                y *= c;

                for (jj = 0; jj<n; jj++)
                {
                    x = V(jj,j);
                    z = V(jj,i);
                    V(jj,j) = x*c + z*s;
                    V(jj,i) = z*c - x*s;
                }

                z = svd::pythagora<T>(f, h);
                W[j] = z;

                // Rotation can be arbitrary if z = 0.
                if (z)
                {
                    z = (T)1.0 / z;
                    c = f*z;
                    s = h*z;
                }

                f = c*g + s*y;
                x = c*y - s*g;

                for (jj = 0; jj<m; jj++)
                {
                    y = A(jj,j);
                    z = A(jj,i);
                    A(jj,j) = y*c + z*s;
                    A(jj,i) = z*c - y*s;
                }
            }

            rv1[l] = 0.0;
            rv1[k] = f;
            W[k] = x;
        }
    }

    //if (sorting != LeaveUnsorted)
    //    Sort<T>(A, W, V, sorting);

    return convergence;
};

namespace svd_test
{

    inline void check_orthonormal(const float3x3 & matrix)
    {
        const float EPSILON = 100.f * std::numeric_limits<float>::epsilon();
        const auto prod = mul(matrix, transpose(matrix));

        for (int i = 0; i < 3; ++i) for (int j = 0; j < 3; ++j)
        {
            if (i == j) assert(std::abs(prod[i][j] - 1) < EPSILON);
            else assert(std::abs(prod[i][j]) < EPSILON);
        }
    }

    inline void run()
    {
        Matrix<float> A(3, 3);
        Matrix<float> V(3, 3);
        std::vector<float> W(3);

        // Row, column
        A(0, 0) = -0.46673855799602715;
        A(1, 0) = 0.67466260360310948;
        A(2, 0) = 0.97646986796448998;

        A(0, 1) = -0.032460753747103721;
        A(1, 1) = 0.046584527749418278;
        A(2, 1) = 0.067431228641151142;

        A(0, 2) = -0.088885055229687815;
        A(1, 2) = 0.1280389179308779;
        A(2, 2) = 0.18532617511453064;

        float3x3 L_A_ORIGINAL = to_linalg(A);

        float maxEntry = 0;
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                maxEntry = std::max(maxEntry, std::abs(A(i, j)));

        const float eps = std::numeric_limits<float>::epsilon();
        const float valueEps = maxEntry * 10.f * eps;

        auto result = singular_value_decomposition<float>(A, 3, 3, W, V);

        float3x3 L_U = to_linalg(A);
        float3x3 L_V = to_linalg(V);

        // --- 
        Matrix<float> S_times_Vt(3, 3);
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                S_times_Vt(i, j) = W[j] * V(i, j);

        float3x3 L_S_times_Vt = transpose(to_linalg(S_times_Vt));

        // --- 

        // --- 
        float3x3 S_times_Vt_Wrong;
        for (int j = 0; j < 3; ++j)
        {
            S_times_Vt_Wrong[j].x = W[j] * L_V[j].x;
            S_times_Vt_Wrong[j].y = W[j] * L_V[j].y;
            S_times_Vt_Wrong[j].z = W[j] * L_V[j].z;
        }

        float3x3 L_S_times_Vt_wrong = transpose(S_times_Vt_Wrong);

        std::cout << "Wrong: " << L_S_times_Vt_wrong << std::endl;
        std::cout << "Passes: " << L_S_times_Vt << std::endl;

        // --- 

        // Verify that the product of the matrices is A:
        const float3x3 product = mul(L_U, L_S_times_Vt);

        for (int i = 0; i < 3; ++i)
        {
            assert(std::abs(product[i].x - L_A_ORIGINAL[i].x) <= valueEps);
            assert(std::abs(product[i].y - L_A_ORIGINAL[i].y) <= valueEps);
            assert(std::abs(product[i].z - L_A_ORIGINAL[i].z) <= valueEps);
        }

        std::cout << determinant(L_U) << std::endl;
        std::cout << determinant(L_V) << std::endl;

        check_orthonormal(L_U);
        check_orthonormal(L_V);

        // Check that U and V are orthogonal:
        //assert(determinant(L_U) > 0.9);
        //assert(determinant(L_V) > 0.9);

        //std::cout << result << std::endl;
    }
}
