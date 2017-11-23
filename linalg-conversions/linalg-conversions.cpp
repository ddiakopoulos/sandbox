#include "index.hpp"
#include "linalg-conversions.hpp"

#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include <Eigen\Geometry>

UniformRandomGenerator generator;

template<class T, int M> void require_zero(const linalg::vec<T, M> & v) { for (int j = 0; j<M; ++j) REQUIRE(v[j] == 0); }
template<class T, int M> void require_approx_equal(const linalg::vec<T, M> & a, const linalg::vec<T, M> & b) { for (int j = 0; j<M; ++j) REQUIRE(a[j] == Approx(b[j]).epsilon(0.01)); }
template<class T, int M, int N> void require_zero(const linalg::mat<T, M, N> & m) { for (int i = 0; i<N; ++i) require_zero(m[i]); }
template<class T, int M, int N> void require_approx_equal(const linalg::mat<T, M, N> & a, const linalg::mat<T, M, N> & b) { for (int i = 0; i<N; ++i) require_approx_equal(a[i], b[i]); }

AffineTransform<float> p;

TEST_CASE("linalg-eigen matrix identity conversions")
{
    float3x3 linalgIdentity3 = Identity3x3;
    Eigen::Matrix<float, 3, 3> eigenIdentity3 = Eigen::Matrix<float, 3, 3>::Identity();
    REQUIRE(linalgIdentity3 == to_linalg(eigenIdentity3));
    REQUIRE(to_eigen(linalgIdentity3) == eigenIdentity3);

    float4x4 linalgIdentity4 = Identity4x4;
    Eigen::Matrix<float, 4, 4> eigenIdentity4 = Eigen::Matrix<float, 4, 4>::Identity(); 
    REQUIRE(linalgIdentity4 == to_linalg(eigenIdentity4));
    REQUIRE(to_eigen(linalgIdentity4) == eigenIdentity4);
}

TEST_CASE("linalg-eigen matrix general conversions")
{
    // Swizzle test on 3x3
    for (int i = 0; i < 1024; ++i)
    {
        float m11 = generator.random_float(1.f);
        float m12 = generator.random_float(1.f);
        float m13 = generator.random_float(1.f);

        float m21 = generator.random_float(1.f);
        float m22 = generator.random_float(1.f);
        float m23 = generator.random_float(1.f);

        float m31 = generator.random_float(1.f);
        float m32 = generator.random_float(1.f);
        float m33 = generator.random_float(1.f);

        float3x3 linalgMatrix = { {m11, m12, m13}, {m21, m22, m23}, {m31, m32, m33} };
        Eigen::Matrix<float, 3, 3> eigenMatrix;
        eigenMatrix(0, 0) = m11;
        eigenMatrix(1, 0) = m12;
        eigenMatrix(2, 0) = m13;
        eigenMatrix(0, 1) = m21;
        eigenMatrix(1, 1) = m22;
        eigenMatrix(2, 1) = m23;
        eigenMatrix(0, 2) = m31;
        eigenMatrix(1, 2) = m32;
        eigenMatrix(2, 2) = m33;

        REQUIRE(linalgMatrix == to_linalg(eigenMatrix));
        REQUIRE(to_eigen(linalgMatrix) == eigenMatrix);
    }

    // Transpose test on 4x4
    for (int i = 0; i < 1024; ++i)
    {
        float m11 = generator.random_float(1.f);
        float m12 = generator.random_float(1.f);
        float m13 = generator.random_float(1.f);
        float m14 = generator.random_float(1.f);

        float m21 = generator.random_float(1.f);
        float m22 = generator.random_float(1.f);
        float m23 = generator.random_float(1.f);
        float m24 = generator.random_float(1.f);

        float m31 = generator.random_float(1.f);
        float m32 = generator.random_float(1.f);
        float m33 = generator.random_float(1.f);
        float m34 = generator.random_float(1.f);

        float m41 = generator.random_float(1.f);
        float m42 = generator.random_float(1.f);
        float m43 = generator.random_float(1.f);
        float m44 = generator.random_float(1.f);

        float4x4 linalgMatrix = { { m11, m12, m13, m14 },{ m21, m22, m23, m24 },{ m31, m32, m33, m34 }, {m41, m42, m43, m44} };
        Eigen::Matrix<float, 4, 4> eigenMatrix;

        eigenMatrix(0, 0) = m11;
        eigenMatrix(1, 0) = m12;
        eigenMatrix(2, 0) = m13;
        eigenMatrix(3, 0) = m14;

        eigenMatrix(0, 1) = m21;
        eigenMatrix(1, 1) = m22;
        eigenMatrix(2, 1) = m23;
        eigenMatrix(3, 1) = m24;

        eigenMatrix(0, 2) = m31;
        eigenMatrix(1, 2) = m32;
        eigenMatrix(2, 2) = m33;
        eigenMatrix(3, 2) = m34;

        eigenMatrix(0, 3) = m41;
        eigenMatrix(1, 3) = m42;
        eigenMatrix(2, 3) = m43;
        eigenMatrix(3, 3) = m44;

        Eigen::Matrix<float, 4, 4> eigenMatrix_T = eigenMatrix.transpose();
        float4x4 linalgMatrix_T = transpose(linalgMatrix);

        REQUIRE(linalgMatrix_T == to_linalg(eigenMatrix_T));
        REQUIRE(to_eigen(linalgMatrix_T) == eigenMatrix_T);
    }
}

TEST_CASE("linalg-eigen vector conversions")
{
    for (int i = 0; i < 1024; ++i)
    {
        float r1 = generator.random_float(std::numeric_limits<float>::min(), std::numeric_limits<float>::max());
        float r2 = generator.random_float(std::numeric_limits<float>::min(), std::numeric_limits<float>::max());

        float2 lf2 = { r1, r2};
        Eigen::Matrix<float, 2, 1> ef2 = { r1, r2 };
        REQUIRE(lf2 == to_linalg(ef2));
        REQUIRE(to_eigen(lf2) == ef2);
    }

    for (int i = 0; i < 1024; ++i)
    {
        float r1 = generator.random_float(std::numeric_limits<float>::min(), std::numeric_limits<float>::max());
        float r2 = generator.random_float(std::numeric_limits<float>::min(), std::numeric_limits<float>::max());
        float r3 = generator.random_float(std::numeric_limits<float>::min(), std::numeric_limits<float>::max());

        float3 lf3 = { r1, r2, r3 };
        Eigen::Matrix<float, 3, 1> ef3 = { r1, r2, r3 };
        REQUIRE(lf3 == to_linalg(ef3));
        REQUIRE(to_eigen(lf3) == ef3);
    }

    for (int i = 0; i < 1024; ++i)
    {
        float r1 = generator.random_float(std::numeric_limits<float>::min(), std::numeric_limits<float>::max());
        float r2 = generator.random_float(std::numeric_limits<float>::min(), std::numeric_limits<float>::max());
        float r3 = generator.random_float(std::numeric_limits<float>::min(), std::numeric_limits<float>::max());
        float r4 = generator.random_float(std::numeric_limits<float>::min(), std::numeric_limits<float>::max());

        float4 lf4 = { r1, r2, r3, r4 };
        Eigen::Matrix<float, 4, 1> ef4 = { r1, r2, r3, r4 };
        REQUIRE(lf4 == to_linalg(ef4));
        REQUIRE(to_eigen(lf4) == ef4);
    }
}

TEST_CASE("linalg-eigen quaternion/rotation conversions")
{
    for (int i = 0; i < 1024; ++i)
    {
        float3 axis = normalize(float3(generator.random_float(1.f), generator.random_float(1.f), generator.random_float(1.f)));
        float angle = generator.random_float_sphere();

        float4 lq = make_rotation_quat_axis_angle(axis, angle);
        float3x3 lrm = get_rotation_submatrix(make_rotation_matrix(lq));

        Eigen::AngleAxis<float> eq = Eigen::AngleAxis<float>(angle, Eigen::Matrix<float, 3, 1>{axis.x, axis.y, axis.z});
        Eigen::Matrix<float, 3, 3> erm = eq.toRotationMatrix();

        require_approx_equal(lrm, to_linalg(erm));
    }
}
