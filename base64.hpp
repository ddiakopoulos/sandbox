#ifndef base_64_hpp
#define base_64_hpp

#include <string>
#include <vector>
#include <stdint.h>

static const std::string b64_characters = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static inline bool is_base64(unsigned char c) { return (isalnum(c) || (c == '+') || (c == '/')); }

inline std::string base64_encode(std::vector<uint8_t> & input) 
{
    unsigned char const * bytes_to_encode = (unsigned char *) input.data();

    std::string output;
    int inputLength = input.size();

    int i{ 0 }, j{ 0 };

    uint8_t char_array_3[3], char_array_4[4];
    while (inputLength--)
    {
        char_array_3[i++] = *(bytes_to_encode++);

        if (i == 3) 
        {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;
            for (i = 0; i < 4; i++) output += b64_characters[char_array_4[i]];
            i = 0;
        }
    }

    if (i)
    {
        for (j = i; j < 3; j++) char_array_3[j] = '\0';
        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        char_array_4[3] = char_array_3[2] & 0x3f;
        for (j = 0; (j < i + 1); j++) output += b64_characters[char_array_4[j]];
        while ((i++ < 3)) output += '=';

    }
    return output;
}

inline std::vector<uint8_t> base64_decode(const std::string & input)
{
    std::vector<uint8_t> output;

    auto inputLength = input.size();
    int inputIdx = 0;
        
    int i{ 0 }, j{ 0 };

    uint8_t char_array_4[4], char_array_3[3];
    while (inputLength-- && (input[inputIdx] != '=') && is_base64(input[inputIdx]))
    {
        char_array_4[i++] = input[inputIdx]; 
        inputIdx++;
        if (i == 4) 
        {
            for (i = 0; i <4; i++) char_array_4[i] = (uint8_t) b64_characters.find(char_array_4[i]);
            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];
            for (i = 0; (i < 3); i++) output.push_back(char_array_3[i]);
            i = 0;
        }
    }

    if (i) 
    {
        for (j = i; j <4; j++) char_array_4[j] = 0;
        for (j = 0; j <4; j++) char_array_4[j] = (uint8_t) b64_characters.find(char_array_4[j]);
        char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
        char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];
        for (j = 0; (j < i - 1); j++) output.push_back(char_array_3[j]);
    }

    return output;
}

/*
    base64.cpp and base64.h

    Copyright (C) 2004-2017 René Nyffenegger

    This source code is provided 'as-is', without any express or implied
    warranty. In no event will the author be held liable for any damages
    arising from the use of this software.

    Permission is granted to anyone to use this software for any purpose,
    including commercial applications, and to alter it and redistribute it
    freely, subject to the following restrictions:

    1. The origin of this source code must not be misrepresented; you must not
    claim that you wrote the original source code. If you use this source code
    in a product, an acknowledgment in the product documentation would be
    appreciated but is not required.

    2. Altered source versions must be plainly marked as such, and must not be
    misrepresented as being the original source code.

    3. This notice may not be removed or altered from any source distribution.
*/

#endif // base_64_hpp
