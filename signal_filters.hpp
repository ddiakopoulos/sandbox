// Inspired by https://github.com/wieden-kennedy/Cinder-Sampling (Apache 2.0)

#ifndef signal_filters_H
#define signal_filters_H

namespace util
{
    
    template<typename T>
    class Filter
    {
    protected:
        T v;
    public:
        T get() const { return v; }
        operator T () { return get(); }
        virtual T update(T const n);
        virtual void reset() = 0;
    }; 

    /*
    * A weighted running average filter. It uses a single weight value, alpha. 
    *   value = ((1.0-alpha) * value) + (alpha * n);
    * Alpha values ~1.0 react quickly, and values ~0.0 react slowly. 
    */

    template<typename T>
    struct SingleExponential : public Filter<T>
    {
        double alpha;

        SingleExponential(double alpha = 0.50) : alpha(alpha)
        {
            
        }

        virtual T update(T const n) override
        {
            Filter<T>::v = ((1.0 - alpha) * Filter<T>::v) + (alpha * T(n));
            return Filter<T>::v;
        }

        virtual void reset() override
        {
            Filter<T>::v = T(0);
        }
    }; 

    /*
    * A weighted running average filter. This calculates a running
    * average for both average and slope using weights alpha + gamma.
    * It is effectively a band-pass filter via dual exponential moving averages. 
    *   value = ((1.0-alpha) * (value + slope)) + (alpha * n);
    *   slope = ((1.0-gamma) * (slope)) + (gamma * (value - value_prev));
    * Weight values ~1.0 react quickly, and values ~0.0 react slowly. 
    */

    template<typename T>
    class DoubleExponential : public Filter<T>
    {
        double slope;

    public:

        double alpha;
        double gamma; 

        DoubleExponential(double alpha = 0.50, double gamma = 1.0) : alpha(alpha), gamma(gamma)
        {
        }

        T update(T const n) override 
        {
            T vP = Filter<T>::v;
            Filter<T>::v = ((1.0 - alpha) * (Filter<T>::v + slope)) + (alpha * T(n));
            slope = ((1.0-gamma) * (slope)) + (gamma * (Filter<T>::v - vP));
            return Filter<T>::v;
        }

        void reset() override
        {
            Filter<T>::v = T(0);
            slope = T(0);
        }
    };

    /*
    * A simple complimentary filter (designed to fuse accelerometer and gyro data) 
    * Reference: http://www.pieter-jan.com/node/11
    * UNIMPLEMENTED
    */

    template<typename T>
    class ComplementaryFilter : public Filter<T>
    {

    public:

        double alpha;
        double gamma; 

        ComplementaryFilter(double alpha = 0.50, double gamma = 1.0) : alpha(alpha), gamma(gamma)
        {
        }

        T update(T const gyroRate, T const accelRate) 
        {
            return Filter<T>::v;
        }

        void reset() override
        {
            Filter<T>::v = T(0);
        }
    };


    /*
    * A simple 1D Linear Kalman Filter
    */

    template<typename T>
    class Kalman1D : public Filter<T>
    {
        double processErrorCovar; // 0 - 1
        double measurementErrorCovar; // 0 - 1

    public:

        double estimateProbability = 0.0; // optionally set initial covariance estimate 

        Kalman1D(double pec = 0.50, double mec = 1.0) : processErrorCovar(pec), measurementErrorCovar(mec)
        {
        }

        T update(T const n) override 
        {

            T value = n; 
            T last = Filter<T>::v;

            T p = estimateProbability + processErrorCovar;
            T kalmanGain = p * ( 1.0 / (p + measurementErrorCovar ) );
            value = value + kalmanGain * ( last - value );
            estimateProbability = ( 1.0 - kalmanGain ) * p;

            Filter<T>::v = value;

            return value; 
        }

        void reset() override
        {
            processErrorCovar = 0.0;
            measurementErrorCovar = 0.0;
            estimateProbability = 0.0;
            Filter<T>::v = T(0);
        }
    };
    
} // end namespace util

#endif
