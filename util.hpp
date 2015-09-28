#ifndef main_util_h
#define main_util_h

#include <sstream>
#include <iostream>

#define ANVIL_PI 3.1415926535897931
#define ANVIL_HALF_PI 1.5707963267948966
#define ANVIL_QUARTER_PI 0.7853981633974483
#define ANVIL_TWO_PI 6.2831853071795862
#define ANVIL_INV_PI 0.3183098861837907
#define ANVIL_INV_TWO_PI 0.1591549430918953
#define ANVIL_INV_HALF_PI 0.6366197723675813

#define ANVIL_DEG_TO_RAD 0.0174532925199433
#define ANVIL_RAD_TO_DEG 57.295779513082321

#define ANVIL_SQRT_2 1.4142135623730951
#define ANVIL_INV_SQRT_2 0.7071067811865475
#define ANVIL_LN_2 0.6931471805599453
#define ANVIL_INV_LN_2 1.4426950408889634
#define ANVIL_LN_10 2.3025850929940459
#define ANVIL_INV_LN_10 0.43429448190325176

#if (defined(__linux) || defined(__unix) || defined(__posix) || defined(__LINUX__) || defined(__linux__))
    #define ANVIL_PLATFORM_LINUX 1
#elif (defined(_WIN64) || defined(_WIN32) || defined(__CYGWIN32__) || defined(__MINGW32__))
    #define ANVIL_PLATFORM_WINDOWS 1
#elif (defined(MACOSX) || defined(__DARWIN__) || defined(__APPLE__))
    #define ANVIL_PLATFORM_OSX 1
#endif

#if (defined(WIN_32) || defined(__i386__) || defined(i386) || defined(__x86__))
    #define ANVIL_ARCH_32 1
#elif (defined(__amd64) || defined(__amd64__) || defined(__x86_64) || defined(__x86_64__) || defined(_M_X64) || defined(__ia64__) || defined(_M_IA64))
    #define ANVIL_ARCH_64 1
#endif

#if (defined(__clang__))
    #define ANVIL_COMPILER_CLANG 1
#elif (defined(__GNUC__))
    #define ANVIL_COMPILER_GCC 1
#elif (defined _MSC_VER)
    #define ANVIL_COMPILER_VISUAL_STUDIO 1
#endif

#if defined (ANVIL_WINDOWS)
    #define GL_PUSH_ALL_ATTRIB() glPushAttrib(GL_ALL_ATTRIB_BITS);
    #define GL_POP_ATTRIB() glPopAttrib();
#else
    #define GL_PUSH_ALL_ATTRIB()
    #define GL_POP_ATTRIB()
#endif

namespace util
{
    struct as_string
    {
        std::ostringstream ss;
        operator std::string() const { return ss.str(); }
        template<class T> as_string & operator << (const T & val) { ss << val; return *this; }
    };
    
    enum class LogChannel : uint8_t { LOG_NADA, LOG_ERROR, LOG_INFO };
    void print_log(LogChannel severity, const char * file, int line, const std::string & message)
    {
        if (severity ==LogChannel::LOG_ERROR) std::cerr << file << " : " << line << " - " << message << std::endl;
        else std::cout << file << " : " << line << " - " << message << std::endl;
    }
}

#define ANVIL_ERROR(...) util::print_log(util::LogChannel::LOG_ERROR, __FILE__, __LINE__, util::as_string() << __VA_ARGS__)
#define ANVIL_INFO(...) util::print_log(util::LogChannel::LOG_INFO, __FILE__, __LINE__, util::as_string() << __VA_ARGS__)

#endif