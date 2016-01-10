#ifndef base_64_h
#define base_64_h

#include <string>

namespace avl
{

    static const std::string b64_characters = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    static inline bool is_base64(unsigned char c) { return (isalnum(c) || (c == '+') || (c == '/')); }

    std::string base64_encode(std::vector<uint8_t> input, uint32_t inputLength) 
    {
        unsigned char const * bytes_to_encode = (unsigned char *) input.data();

        std::string output;

        int i = 0;
        int j = 0;
        unsigned char char_array_3[3];
        unsigned char char_array_4[4];

        while (inputLength--)
        {
            char_array_3[i++] = *(bytes_to_encode++);
            if (i == 3) 
            {
                char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
                char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
                char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
                char_array_4[3] = char_array_3[2] & 0x3f;

                for (i = 0; i < 4; i++)
                    output += b64_characters[char_array_4[i]];
                i = 0;
            }
        }

        if (i)
        {
            for (j = i; j < 3; j++)
                char_array_3[j] = '\0';

            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for (j = 0; (j < i + 1); j++)
                output += b64_characters[char_array_4[j]];

            while ((i++ < 3))
                output += '=';

        }
        return output;
    }

    std::string base64_decode(const std::string input) 
    {
        auto inputLength = input.size();
        int inputIdx = 0;
        
        int i = 0;
        int j = 0;

        unsigned char char_array_4[4], char_array_3[3];

        std::string output ;

        while (inputLength-- && (input[inputIdx] != '=') && is_base64(input[inputIdx]))
        {
            char_array_4[i++] = input[inputIdx]; 
            inputIdx++;
            if (i == 4) 
            {
                for (i = 0; i <4; i++)
                    char_array_4[i] = b64_characters.find(char_array_4[i]);

                char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
                char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
                char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

                for (i = 0; (i < 3); i++)
                    output  += char_array_3[i];
                i = 0;
            }
        }

        if (i) 
        {
            for (j = i; j <4; j++)
                char_array_4[j] = 0;

            for (j = 0; j <4; j++)
                char_array_4[j] = b64_characters.find(char_array_4[j]);

            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

            for (j = 0; (j < i - 1); j++) output  += char_array_3[j];
        }

        return output;
    }

}

#endif // base_64_h
