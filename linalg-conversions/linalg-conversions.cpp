#include "index.hpp"
#include "linalg-conversions.hpp"

#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include <Eigen\Geometry>

UniformRandomGenerator generator;

template<class T, int M> void require_zero(const linalg::vec<T, M> & v) { for (int j = 0; j<M; ++j) REQUIRE(v[j] == 0); }
template<class T, int M> void require_approx_equal(const linalg::vec<T, M> & a, const linalg::vec<T, M> & b) { for (int j = 0; j<M; ++j) REQUIRE(a[j] == Approx(b[j]).epsilon(0.0001)); }
template<class T, int M, int N> void require_zero(const linalg::mat<T, M, N> & m) { for (int i = 0; i<N; ++i) require_zero(m[i]); }
template<class T, int M, int N> void require_approx_equal(const linalg::mat<T, M, N> & a, const linalg::mat<T, M, N> & b) { for (int i = 0; i<N; ++i) require_approx_equal(a[i], b[i]); }

TEST_CASE("linalg-eigen matrix identity conversions")
{
    float3x3 linalgIdentity3 = Identity3x3;
    Eigen::Matrix<float, 3, 3> eigenIdentity3 = Eigen::Matrix<float, 3, 3>::Identity();
    REQUIRE(linalgIdentity3 == to_linalg(eigenIdentity3));

    float4x4 linalgIdentity4 = Identity4x4;
    Eigen::Matrix<float, 4, 4> eigenIdentity4 = Eigen::Matrix<float, 4, 4>::Identity(); 
    REQUIRE(linalgIdentity4 == to_linalg(eigenIdentity4));
}

TEST_CASE("linalg-eigen vector conversions")
{
    for (int i = 0; i < 1024; ++i)
    {
        float r1 = generator.random_float(-1000.f, 1000.f);
        float r2 = generator.random_float(-1000.f, 1000.f);

        float2 lf2 = { r1, r2};
        Eigen::Matrix<float, 2, 1> ef2 = { r1, r2 };
        REQUIRE(lf2 == to_linalg(ef2));
    }

    for (int i = 0; i < 1024; ++i)
    {
        float r1 = generator.random_float(-1000.f, 1000.f);
        float r2 = generator.random_float(-1000.f, 1000.f);
        float r3 = generator.random_float(-1000.f, 1000.f);

        float3 lf3 = { r1, r2, r3 };
        Eigen::Matrix<float, 3, 1> ef3 = { r1, r2, r3 };
        REQUIRE(lf3 == to_linalg(ef3));
    }

    for (int i = 0; i < 1024; ++i)
    {
        float r1 = generator.random_float(-1000.f, 1000.f);
        float r2 = generator.random_float(-1000.f, 1000.f);
        float r3 = generator.random_float(-1000.f, 1000.f);
        float r4 = generator.random_float(-1000.f, 1000.f);

        float4 lf4 = { r1, r2, r3, r4 };
        Eigen::Matrix<float, 4, 1> ef4 = { r1, r2, r3, r4 };
        REQUIRE(lf4 == to_linalg(ef4));
    }
}

TEST_CASE("linalg-eigen quaternion conversions")
{
    for (int i = 0; i < 1024; ++i)
    {
        float r1 = generator.random_float(1.f);
        float r2 = generator.random_float(1.f);
        float r3 = generator.random_float(1.f);

        float3 axis = normalize(float3(r1, r2, r3));
        float angle = generator.random_float_sphere();

        float4 lq = make_rotation_quat_axis_angle(axis, angle);
        float3x3 lrm = get_rotation_submatrix(make_rotation_matrix(lq));

        Eigen::AngleAxis<float> eq = Eigen::AngleAxis<float>(angle, Eigen::Matrix<float, 3, 1>{axis.x, axis.y, axis.z});
        Eigen::Matrix<float, 3, 3> erm = eq.toRotationMatrix();

        require_approx_equal(lrm, to_linalg(erm));
    }
}


