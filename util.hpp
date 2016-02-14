#ifndef main_util_h
#define main_util_h

#include <sstream>
#include <iostream>
#include <vector>
#include "linear_algebra.hpp"

#define ANVIL_PI            3.1415926535897931
#define ANVIL_HALF_PI       1.5707963267948966
#define ANVIL_QUARTER_PI    0.7853981633974483
#define ANVIL_TWO_PI        6.2831853071795862
#define ANVIL_TAU           ANVIL_TWO_PI
#define ANVIL_INV_PI        0.3183098861837907
#define ANVIL_INV_TWO_PI    0.1591549430918953
#define ANVIL_INV_HALF_PI   0.6366197723675813

#define ANVIL_DEG_TO_RAD    0.0174532925199433
#define ANVIL_RAD_TO_DEG    57.295779513082321

#define ANVIL_SQRT_2        1.4142135623730951
#define ANVIL_INV_SQRT_2    0.7071067811865475
#define ANVIL_LN_2          0.6931471805599453
#define ANVIL_INV_LN_2      1.4426950408889634
#define ANVIL_LN_10         2.3025850929940459
#define ANVIL_INV_LN_10     0.43429448190325176

#define ANVIL_GOLDEN        1.61803398874989484820

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

#if defined (ANVIL_PLATFORM_WINDOWS)
    #define GL_PUSH_ALL_ATTRIB() glPushAttrib(GL_ALL_ATTRIB_BITS);
    #define GL_POP_ATTRIB() glPopAttrib();
#else
    #define GL_PUSH_ALL_ATTRIB();
    #define GL_POP_ATTRIB();
#endif

static const avl::float4x4 Identity4x4 = {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}};
static const avl::float3x3 Identity3x3 = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
static const avl::float2x2 Identity2x2 = {{1, 0}, {0, 1}};

static const avl::float4x4 Zero4x4 = {{0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}};
static const avl::float3x3 Zero3x3 = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}};
static const avl::float2x2 Zero2x2 = {{0, 0}, {0, 0}};

namespace avl
{
    struct as_string
    {
        std::ostringstream ss;
        operator std::string() const { return ss.str(); }
        template<class T> as_string & operator << (const T & val) { ss << val; return *this; }
    };
    
    enum class LogChannel : uint8_t { LOG_SILENT, LOG_ERROR, LOG_INFO };
    
    inline void print_log(LogChannel severity, const char * file, int line, const std::string & message)
    {
        if (severity == LogChannel::LOG_ERROR) std::cerr << file << " : " << line << " - " << message << std::endl;
        else std::cout << file << " : " << line << " - " << message << std::endl;
    }
    
    class Noncopyable
    {
    protected:
        Noncopyable() = default;
        ~Noncopyable() = default;
        Noncopyable (const Noncopyable& r) = delete;
        Noncopyable & operator = (const Noncopyable& r) = delete;
    };
    
    inline std::string codepoint_to_utf8(uint32_t codepoint)
    {
        int n = 0;
        if (codepoint < 0x80) n = 1;
        else if (codepoint < 0x800) n = 2;
        else if (codepoint < 0x10000) n = 3;
        else if (codepoint < 0x200000) n = 4;
        else if (codepoint < 0x4000000) n = 5;
        else if (codepoint <= 0x7fffffff) n = 6;
        
        std::string str(n, ' ');
        switch (n)
        {
            case 6: str[5] = 0x80 | (codepoint & 0x3f); codepoint = codepoint >> 6; codepoint |= 0x4000000;
            case 5: str[4] = 0x80 | (codepoint & 0x3f); codepoint = codepoint >> 6; codepoint |= 0x200000;
            case 4: str[3] = 0x80 | (codepoint & 0x3f); codepoint = codepoint >> 6; codepoint |= 0x10000;
            case 3: str[2] = 0x80 | (codepoint & 0x3f); codepoint = codepoint >> 6; codepoint |= 0x800;
            case 2: str[1] = 0x80 | (codepoint & 0x3f); codepoint = codepoint >> 6; codepoint |= 0xc0;
            case 1: str[0] = codepoint;
        }
        
        return str;
    }
    
    inline void flip_image(unsigned char * pixels, const uint32_t width, const uint32_t height, const uint32_t bytes_per_pixel)
    {
        const size_t stride = width * bytes_per_pixel;
        std::vector<unsigned char> row(stride);
        unsigned char * low = pixels;
        unsigned char * high = &pixels[(height - 1) * stride];
        
        for (; low < high; low += stride, high -= stride)
        {
            memcpy(row.data(), low, stride);
            memcpy(low, high, stride);
            memcpy(high, row.data(), stride);
        }
    }
}

#define ANVIL_ERROR(...) avl::print_log(avl::LogChannel::LOG_ERROR, __FILE__, __LINE__, avl::as_string() << __VA_ARGS__)
#define ANVIL_INFO(...) avl::print_log(avl::LogChannel::LOG_INFO, __FILE__, __LINE__, avl::as_string() << __VA_ARGS__)

#endif
