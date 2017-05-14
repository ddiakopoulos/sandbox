#pragma once

#ifndef nvg_util_h
#define nvg_util_h

#include "gl-api.hpp"
#include "third_party/nanovg.h"

#include <stdint.h>
#include <vector>
#include <string>
#include <stdlib.h>

class NvgFont
{
    std::vector<uint8_t> buffer;
    NVGcontext * nvg;
public:
    int id;
    NvgFont(NVGcontext * nvg, const std::string & name, std::vector<uint8_t> buffer);
    size_t get_cursor_location(const std::string & text, float fontSize, int xCoord) const;
};

NVGcontext * make_nanovg_context(int flags);
void release_nanovg_context(NVGcontext * context);

#endif