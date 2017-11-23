#ifndef linalg_conversions_hpp
#define linalg_conversions_hpp

#include "linalg_util.hpp"

//////////////////////////////////////////////////////////////
// Eigen to OpenCV Conversions for Vectors and NxM Matrices //
//////////////////////////////////////////////////////////////

#ifdef HAVE_OPENCV

#include <opencv2/core/core.hpp>
#include <Eigen/Core>

template <typename scalar_t>
inline cv::Vec<scalar_t, 2> eigen_to_cv(const Eigen::Matrix<scalar_t, 2, 1> & v)
{
	return cv::Vec<scalar_t, 2>(v(0), v(1));
}

template <typename scalar_t>
inline cv::Vec<scalar_t, 3>eigen_to_cv(const Eigen::Matrix<scalar_t, 3, 1> & v)
{
	return cv::Vec<scalar_t, 3>(v(0), v(1), v(2));
}

template <typename scalar_t>
inline cv::Vec<scalar_t, 4> eigen_to_cv(const Eigen::Matrix<scalar_t, 4, 1> & v)
{
	return cv::Vec<scalar_t, 4>(v(0), v(1), v(2), v(3));
}

inline cv::Point2f eigen_to_cv(const Eigen::Vector2f & v)
{
	return cv::Point2f(v(0), v(1));
}

inline cv::Point3f eigen_to_cv(const Eigen::Vector3f & v)
{
	return cv::Point3f(v(0), v(1), v(2));
}

template <typename scalar_t, int H, int W>
inline cv::Mat_<scalar_t> eigen_to_cv(const Eigen::Matrix<scalar_t, H, W> & ep)
{
	cv::Mat cvMat(H, W, CV_32F);
	for (int r = 0; r < H; ++r)
	{
		for (int c = 0; c < W; ++c)
		{
			cvMat.at<scalar_t>(r, c) = ep(r, c);
		}
	}
	return cvMat;
}

//////////////////////////////////////////////////////////////
// OpenCV to Eigen Conversions for Vectors and NxM Matrices //
//////////////////////////////////////////////////////////////

template <typename scalar_t>
inline Eigen::Matrix<scalar_t, 2, 1> cv_to_eigen(const cv::Vec<scalar_t, 2> & v)
{
	return Eigen::Matrix<scalar_t, 2, 1>(v[0], v[1]);
}

template <typename scalar_t>
inline Eigen::Matrix<scalar_t, 3, 1> cv_to_eigen(const cv::Vec<scalar_t, 3> & v)
{
	return Eigen::Matrix<scalar_t, 3, 1>(v[0], v[1], v[2]);
}

template <typename scalar_t>
inline Eigen::Matrix<scalar_t, 4, 1> cv_to_eigen(const cv::Vec<scalar_t, 4> & v)
{
	return Eigen::Matrix<scalar_t, 4, 1>(v[0], v[1], v[2], v[3]);
}

inline Eigen::Vector2f cv_to_eigen(const cv::Point2f & v)
{
	return Eigen::Vector2f(v.x, v.y);
}

inline Eigen::Vector3f cv_to_eigen(const cv::Point3f & v)
{
	return Eigen::Vector3f(v.x, v.y, v.z);
}

template <typename scalar_t, int H, int W>
inline Eigen::Matrix<scalar_t, H, W> cv_to_eigen(const cv::Mat_<scalar_t> & cvMat)
{
	Eigen::Matrix<scalar_t, H, W> eigenMat;
	for (int r = 0; r < H; ++r)
	{
		for (int c = 0; c < W; ++c)
		{
			eigenMat(r, c) = cvMat.at<scalar_t>(r, c);
		}
	}
	return eigenMat;
}

#endif

////////////////////////////////////////////////////////////////
// Linalg.h to Eigen Conversions for Vectors and NxM Matrices //
////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////
// GLM to Eigen Conversions for Vectors and NxM Matrices //
///////////////////////////////////////////////////////////


#endif // linalg_conversions_hpp
