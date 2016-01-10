// See COPYING file for attribution information

#ifndef one_euro_filter_h
#define one_euro_filter_h

#include "linear_algebra.hpp"

namespace avl
{
    template<int dimension = 3, typename Scalar = float>
    class LowPassFilter
    {
    public:

        typedef Scalar scalar_type;
        typedef Scalar value_type[dimension];
        typedef const scalar_type *return_type;

        LowPassFilter() : _firstTime(true) { }

        void reset()
        {
            _firstTime = true;
        }

        return_type filter(const value_type x, scalar_type alpha)
        {
            if (_firstTime)
            {
                _firstTime = false;
                memcpy(_hatxprev, x, sizeof(_hatxprev));
            }

            value_type hatx;
            for (int i = 0; i < dimension; ++i)
            {
                hatx[i] = alpha * x[i] + (1 - alpha) * _hatxprev[i];
            }

            memcpy(_hatxprev, hatx, sizeof(_hatxprev));
            return _hatxprev;
        }

        return_type hatxprev()
        {
            return _hatxprev;
        }

    private:

        bool _firstTime;
        value_type _hatxprev;

    };

    typedef LowPassFilter<> LowPassFilterVec;

    template<int dimension = 3, typename Scalar = float>
    class VectorFilterable
    {

    public:

        typedef Scalar scalar_type;
        typedef Scalar value_type[dimension];
        typedef value_type derivative_value_type;
        typedef Scalar *value_ptr_type;
        typedef LowPassFilter<dimension, Scalar> value_filter_type;
        typedef LowPassFilter<dimension, Scalar> derivative_filter_type;
        typedef typename value_filter_type::return_type value_filter_return_type;

        static void setDxIdentity(value_ptr_type dx)
        {
            for (int i = 0; i < dimension; ++i)
            {
                dx[i] = 0;
            }
        }

        static void computeDerivative(derivative_value_type dx, value_filter_return_type prev, const value_type current, scalar_type dt)
        {
            for (int i = 0; i < dimension; ++i)
            {
                dx[i] = (current[i] - prev[i]) / dt;
            }
        }
        static scalar_type computeDerivativeMagnitude(derivative_value_type const dx)
        {
            scalar_type sqnorm = 0;
            for (int i = 0; i < dimension; ++i)
            {
                sqnorm += dx[i] * dx[i];
            }
            return sqrtf(sqnorm);
        }

    };

    template<typename Filterable = VectorFilterable<> >
    class OneEuroFilter
    {

    public:

        typedef Filterable contents;
        typedef typename Filterable::scalar_type scalar_type;
        typedef typename Filterable::value_type value_type;
        typedef typename Filterable::derivative_value_type derivative_value_type;
        typedef typename Filterable::value_ptr_type value_ptr_type;
        typedef typename Filterable::derivative_filter_type derivative_filter_type;
        typedef typename Filterable::value_filter_type value_filter_type;
        typedef typename value_filter_type::return_type value_filter_return_type;

        OneEuroFilter(scalar_type mincutoff, scalar_type beta, scalar_type dcutoff) :
            _firstTime(true),
            _mincutoff(mincutoff), _dcutoff(dcutoff),
            _beta(beta) {};

        OneEuroFilter() : _firstTime(true), _mincutoff(1), _dcutoff(1), _beta(0.5) {};

        void reset()
        {
            _firstTime = true;
        }

        void setMinCutoff(scalar_type mincutoff)
        {
            _mincutoff = mincutoff;
        }

        scalar_type getMinCutoff() const
        {
            return _mincutoff;
        }

        void setBeta(scalar_type beta)
        {
            _beta = beta;
        }

        scalar_type getBeta() const
        {
            return _beta;
        }

        void setDerivativeCutoff(scalar_type dcutoff)
        {
            _dcutoff = dcutoff;
        }

        scalar_type getDerivativeCutoff() const
        {
            return _dcutoff;
        }

        void setParams(scalar_type mincutoff, scalar_type beta, scalar_type dcutoff)
        {
            _mincutoff = mincutoff;
            _beta = beta;
            _dcutoff = dcutoff;
        }

        const value_filter_return_type filter(scalar_type dt, const value_type x)
        {
            derivative_value_type dx;

            if (_firstTime)
            {
                _firstTime = false;
                Filterable::setDxIdentity(dx);
            }
            else
            {
                Filterable::computeDerivative(dx, _xfilt.hatxprev(), x, dt);
            }

            scalar_type derivative_magnitude = Filterable::computeDerivativeMagnitude(_dxfilt.filter(dx, alpha(dt, _dcutoff)));
            scalar_type cutoff = _mincutoff + _beta * derivative_magnitude;

            auto returnedVal = _xfilt.filter(x, alpha(dt, cutoff));
            return returnedVal;
        }

    private:

        static scalar_type alpha(scalar_type dt, scalar_type cutoff)
        {
            scalar_type tau = scalar_type(1) / (scalar_type(2) * M_PI * cutoff);
            return scalar_type(1) / (scalar_type(1) + tau / dt);
        }

        bool _firstTime;
        scalar_type _mincutoff, _dcutoff;
        scalar_type _beta;
        value_filter_type _xfilt;
        derivative_filter_type _dxfilt;

    };

    typedef OneEuroFilter<> OneEuroFilterVec;

    class LowPassFilterQuat
    {

    public:

        typedef const math::float4 return_type;

        LowPassFilterQuat() : _firstTime(true) { }

        return_type filter(const math::float4 x, float alpha)
        {
            if (_firstTime)
            {
                _firstTime = false;
                _hatxprev = x;
            }

            math::float4 hatx;

            // destination, start, end, alpha
            hatx = math::qlerp(_hatxprev, x, alpha);

            // destination, source
            _hatxprev = hatx;

            return _hatxprev;
        }

        return_type hatxprev()
        {
            return _hatxprev;
        }

    private:

        bool _firstTime;
        math::float4 _hatxprev;
    };

    class QuatFilterable
    {

    public:

        typedef float scalar_type;

        typedef math::vec<scalar_type,4> value_type;
        typedef math::vec<scalar_type,4> derivative_value_type;
        typedef math::vec<scalar_type,4> value_ptr_type;

        typedef LowPassFilterQuat value_filter_type;
        typedef LowPassFilterQuat derivative_filter_type;
        typedef value_filter_type::return_type value_filter_return_type;

        static void setDxIdentity(value_ptr_type &dx)
        {
            dx = {0, 0, 0, 1};
        }

        static void computeDerivative(derivative_value_type dx, value_filter_return_type prev, const value_type current, scalar_type dt)
        {
            scalar_type rate = 1.0f / dt;

            math::float4 ip = math::qinv(prev);

            math::vec<scalar_type, 4> inverse_prev((scalar_type)ip.x, (scalar_type)ip.y, (scalar_type)ip.z, (scalar_type)ip.w);

            // computes quaternion product destQuat = qLeft * qRight.
            dx = current * inverse_prev;

            // nlerp instead of slerp
            dx.x *= rate;
            dx.y *= rate;
            dx.z *= rate;
            dx.w = dx.w * rate + (1.0f - rate);

            dx = math::normalize(dx);
        }

        static scalar_type computeDerivativeMagnitude(derivative_value_type const dx)
        {
            // Should be safe since the quaternion we're given has been normalized.
            return 2.0f * acosf(static_cast<float>(dx.w));
        }

    };

    typedef OneEuroFilter<QuatFilterable> OneEuroFilterQuat;

};

#endif // end one_euro_filter_h