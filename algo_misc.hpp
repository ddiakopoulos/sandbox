#ifndef algo_misc_hpp
#define algo_misc_hpp

#include "util.hpp"
#include <functional>
#include "linalg_util.hpp"

struct SimpleHarmonicOscillator
{
    float frequency = 0, amplitude = 0, phase = 0;
    float value() const { return std::sin(phase) * amplitude; }
    void update(float timestep) { phase += frequency * timestep; }
};

inline std::vector<bool> make_euclidean_pattern(int steps, int pulses)
{
    std::vector<bool> pattern;

    std::function<void(int, int, std::vector<bool> &, std::vector<int> &, std::vector<int> &)> bjorklund;

    bjorklund = [&bjorklund](int level, int r, std::vector<bool> & pattern, std::vector<int> & counts, std::vector<int> & remainders)
    {
        r++;
        if (level > -1)
        {
            for (int i = 0; i < counts[level]; ++i) bjorklund(level - 1, r, pattern, counts, remainders);
            if (remainders[level] != 0) bjorklund(level - 2, r, pattern, counts, remainders);
        }
        else if (level == -1) pattern.push_back(false);
        else if (level == -2) pattern.push_back(true);
    };

    if (pulses > steps || pulses == 0 || steps == 0) return pattern;

    std::vector<int> counts;
    std::vector<int> remainders;

    int divisor = steps - pulses;
    remainders.push_back(pulses);
    int level = 0;

    while (true)
    {
        counts.push_back(divisor / remainders[level]);
        remainders.push_back(divisor % remainders[level]);
        divisor = remainders[level];
        level++;
        if (remainders[level] <= 1) break;
    }

    counts.push_back(divisor);

    bjorklund(level, 0, pattern, counts, remainders);

    return pattern;
}

#endif // end algo_misc_hpp
