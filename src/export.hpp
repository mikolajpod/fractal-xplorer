#pragma once

#include "renderer.hpp"

#include <string>

// Returns empty string on success, or an error message on failure.
std::string export_png(const char* path, const PixelBuffer& buf);

#ifdef HAVE_JXL
std::string export_jxl(const char* path, const PixelBuffer& buf);
#endif

// True when compiled with JPEG XL support.
inline bool jxl_available()
{
#ifdef HAVE_JXL
    return true;
#else
    return false;
#endif
}
