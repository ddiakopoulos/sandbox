// See COPYING file for attribution information

#ifndef dsp_filters_H
#define dsp_filters_H

namespace avl
{
 
    template<typename T>
    class Filter
    {
    protected:
        T v;
    public:
        T get() const { return v; }
        operator T () { return get(); }
        virtual T update(T const n) = 0;
        virtual void reset() = 0;
    }; 

    /*
    * A weighted running average filter. It uses a single weight value, alpha. 
    *   value = ((1.0-alpha) * value) + (alpha * n);
    * Alpha values ~1.0 react quickly, and values ~0.0 react slowly. 
    */

    template<typename T>
    struct SingleExponential final : public Filter<T>
    {
        double alpha;

        SingleExponential(double alpha = 0.50) : alpha(alpha) { }

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
    class DoubleExponential final : public Filter<T>
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
    * A simple 1D Linear Kalman Filter
    */

    template<typename T>
    class Kalman1D final : public Filter<T>
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

    /*
    * A simple complimentary filter (designed to fuse accelerometer and gyro data)
    * Reference: http://philstech.blogspot.com/2015/06/complimentary-filter-example-quaternion.html
    */
    class ComplementaryFilterQuaternion
    {
        float4 value = float4(0, 0, 0, 1);
        float3 correctedBody = float3(0, 0, 0);
        float3 correctedWorld = float3(0, 0, 0);
        float3 accelWorld = float3(0, 0, 0);
        float3 worldUp = float3(0, 0, 0);
    public:

        ComplementaryFilterQuaternion(float3 worldUp = { 0.0f, 1.0f, 0.0f }) : worldUp(worldUp) { }

        float4 update(float3 gyro, float3 accelBody, const float samplePeriod)
        {
            accelWorld = qrot(value, accelBody);
            correctedWorld = cross(accelWorld, worldUp); // compute error
            correctedBody = qrot(qconj(value), correctedWorld); // rotate correction back to body
            gyro = gyro + correctedBody;  // add correction vector to gyro
            float4 incrementalRotation = float4(gyro, samplePeriod);
            value = qmul(incrementalRotation, value); // integrate quaternion
            return value;
        }
    };
    
}

#endif // end dsp_filters_h
