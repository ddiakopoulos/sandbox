// MurmurHash3 was written by Austin Appleby, and is placed in the public
// domain. The author hereby disclaims copyright to this source code.

#ifndef murmur_hash_3
#define murmur_hash_3

#if defined(_MSC_VER)
    typedef unsigned char uint8_t;
    typedef unsigned long uint32_t;
    typedef unsigned __int64 uint64_t;
#else
    #include <stdint.h>
#endif

void MurmurHash3_x86_32 (const void * key, int len, uint32_t seed, void * out);
void MurmurHash3_x86_128 (const void * key, int len, uint32_t seed, void * out);
void MurmurHash3_x64_128 (const void * key, int len, uint32_t seed, void * out);

#endif // murmur_hash_3
