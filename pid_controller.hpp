// Based on https://github.com/BlockoS/PID/blob/ae619ca4dc0552094ea97327a6e034151768b343/PID.inl (Apache 2.0)

#ifndef pid_controller_h
#define pid_controller_h

#include "time_keeper.hpp"
#include "math_util.hpp"
#include <iostream>
#include <limits>

namespace math
{

class PIDController
{

    // https://en.wikipedia.org/wiki/Simpson's_rule
    struct Integrator
    {
        const double integrate(const double & error, const double & delta_seconds)
        {
            lastError[2] = lastError[1];
            lastError[1] = lastError[0];
            lastError[0] = error;
            lastOutput[2] = lastOutput[1];
            lastOutput[1] = lastOutput[0];
            lastOutput[0] = lastOutput[1] + (1.0 / rest) * (lastError[0] / 6.0 + 4.0 * lastError[1] / 6.0 + lastError[2] / 6.0) * delta_seconds;
            return lastOutput[0];
        }

        double max_limit;
        double min_limit; 
        double rest = 0.0;

        double lastError[3] = {0, 0, 0};
        double lastOutput[3] = {0, 0, 0};
    };

public:

    double p = 0.0;
    double i = 0.0;
    double d = 0.0;

    double clamped_max = -std::numeric_limits<float>::max();
    double clamped_min = std::numeric_limits<float>::min();

    double setPoint = 0.0; 
    double lastInput = 0.0;

    PIDController()
    {
        set_anti_windup(-1024, 1024);
        timer.start();
    }

    ~PIDController()
    {
        timer.stop();
    }

    void set_anti_windup(const double min, const double max)
    {
        integrator.min_limit = min;
        integrator.max_limit = max;
    }

    double update(const double input)
    {
        const double time_epsilon = 1e-6;

        auto deltaSeconds = timer.seconds().count();
        timer.reset();

        lastInput = input;

        double error = setPoint - lastInput;

        double p_output = error * p;
        double i_output = i * integrator.integrate(error, deltaSeconds);

        if (deltaSeconds <= time_epsilon)
            return (p_output + i_output);

        double d_output = (error - runningError) / deltaSeconds * d;
        double output = p_output + i_output + d_output;

        output = clamp(output, clamped_min, clamped_max);

        return output;
    }

 private:

    Integrator integrator;
    double runningError = 0.0;
    util::TimeKeeper timer;

};

template<class C, class R> std::basic_ostream<C, R> & operator << (std::basic_ostream<C, R> & ss, const PIDController & v)
{
  // @todo
}


} // end namespace math

#endif // pid_controller_h
