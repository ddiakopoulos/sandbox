#ifndef string_utils_h
#define string_utils_h

#include <string>
#include <sstream>
#include <vector>
#include <functional>
#include <cctype>
#include <algorithm>

namespace util
{

    inline std::string trim_left(std::string s)
    {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), std::not1(std::ptr_fun<int, int>(std::isspace))));
        return s;
    }

    inline std::string trim_right(std::string s)
    {
        s.erase(std::find_if(s.rbegin(), s.rend(), std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
        return s;
    }

    inline std::string trim(std::string s)
    {
        return trim_left(trim_right(s));
    }

    inline std::vector<std::string> split(const std::string & s, char delim) 
    {
        std::vector<std::string> list;
        std::stringstream ss(s);
        std::string item;
        while (std::getline(ss, item, delim)) 
            list.push_back(item);
        return list;
    }

} // end namespace util

#endif // string_utils_h
