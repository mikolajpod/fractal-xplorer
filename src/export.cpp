#include "export.hpp"

#include <png.h>
#include <cstdio>
#include <cstring>

#ifdef HAVE_JXL
#include <jxl/encode.h>
#include <jxl/color_encoding.h>
#include <vector>
#endif

// ---------------------------------------------------------------------------
// PNG export
//
// Pixel layout: each uint32_t stores 0xAA BB GG RR.
// On a little-endian machine the bytes in memory are [R, G, B, A], which is
// exactly what PNG_COLOR_TYPE_RGBA expects — no conversion needed.
// ---------------------------------------------------------------------------
std::string export_png(const char* path, const PixelBuffer& buf)
{
    FILE* fp = std::fopen(path, "wb");
    if (!fp)
        return std::string("Cannot open file for writing: ") + path;

    png_structp png = png_create_write_struct(
        PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (!png) {
        std::fclose(fp);
        return "png_create_write_struct failed";
    }

    png_infop info = png_create_info_struct(png);
    if (!info) {
        png_destroy_write_struct(&png, nullptr);
        std::fclose(fp);
        return "png_create_info_struct failed";
    }

    if (setjmp(png_jmpbuf(png))) {
        png_destroy_write_struct(&png, &info);
        std::fclose(fp);
        return "PNG write error (libpng longjmp)";
    }

    png_init_io(png, fp);
    png_set_IHDR(png, info,
                 static_cast<png_uint_32>(buf.width),
                 static_cast<png_uint_32>(buf.height),
                 8, PNG_COLOR_TYPE_RGBA,
                 PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT,
                 PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png, info);

    for (int y = 0; y < buf.height; ++y) {
        const png_const_bytep row =
            reinterpret_cast<png_const_bytep>(buf.pixels.data() + y * buf.width);
        png_write_row(png, row);
    }

    png_write_end(png, nullptr);
    png_destroy_write_struct(&png, &info);
    std::fclose(fp);
    return {};  // success
}

// ---------------------------------------------------------------------------
// JPEG XL export (lossless RGBA, 8-bit)
// ---------------------------------------------------------------------------
#ifdef HAVE_JXL
std::string export_jxl(const char* path, const PixelBuffer& buf)
{
    JxlEncoder* enc = JxlEncoderCreate(nullptr);
    if (!enc) return "JxlEncoderCreate failed";

    // Basic image info
    JxlBasicInfo bi;
    JxlEncoderInitBasicInfo(&bi);
    bi.xsize                     = static_cast<uint32_t>(buf.width);
    bi.ysize                     = static_cast<uint32_t>(buf.height);
    bi.bits_per_sample           = 8;
    bi.exponent_bits_per_sample  = 0;
    bi.alpha_bits                = 8;
    bi.alpha_exponent_bits       = 0;
    bi.num_color_channels        = 3;
    bi.num_extra_channels        = 1;
    bi.uses_original_profile     = JXL_TRUE;

    if (JxlEncoderSetBasicInfo(enc, &bi) != JXL_ENC_SUCCESS) {
        JxlEncoderDestroy(enc);
        return "JxlEncoderSetBasicInfo failed";
    }

    // Alpha extra-channel info
    JxlExtraChannelInfo eci;
    JxlEncoderInitExtraChannelInfo(JXL_CHANNEL_ALPHA, &eci);
    eci.bits_per_sample          = 8;
    eci.exponent_bits_per_sample = 0;
    if (JxlEncoderSetExtraChannelInfo(enc, 0, &eci) != JXL_ENC_SUCCESS) {
        JxlEncoderDestroy(enc);
        return "JxlEncoderSetExtraChannelInfo failed";
    }

    // sRGB colour encoding
    JxlColorEncoding color;
    JxlColorEncodingSetToSRGB(&color, /*is_gray=*/JXL_FALSE);
    if (JxlEncoderSetColorEncoding(enc, &color) != JXL_ENC_SUCCESS) {
        JxlEncoderDestroy(enc);
        return "JxlEncoderSetColorEncoding failed";
    }

    // Frame settings: lossless
    JxlEncoderFrameSettings* opts = JxlEncoderFrameSettingsCreate(enc, nullptr);
    if (JxlEncoderSetFrameLossless(opts, JXL_TRUE) != JXL_ENC_SUCCESS) {
        JxlEncoderDestroy(enc);
        return "JxlEncoderSetFrameLossless failed";
    }

    // Add image frame (raw RGBA bytes — see palette.cpp for layout)
    JxlPixelFormat fmt = {4, JXL_TYPE_UINT8, JXL_NATIVE_ENDIAN, 0};
    const size_t data_size = static_cast<size_t>(buf.width) * buf.height * 4;
    if (JxlEncoderAddImageFrame(opts, &fmt, buf.pixels.data(), data_size)
            != JXL_ENC_SUCCESS) {
        JxlEncoderDestroy(enc);
        return "JxlEncoderAddImageFrame failed";
    }
    JxlEncoderCloseInput(enc);

    // Collect compressed output
    std::vector<uint8_t> output(65536);
    uint8_t* next_out  = output.data();
    size_t   avail_out = output.size();
    JxlEncoderStatus status;
    while ((status = JxlEncoderProcessOutput(enc, &next_out, &avail_out))
               == JXL_ENC_NEED_MORE_OUTPUT) {
        const size_t used = next_out - output.data();
        output.resize(output.size() * 2);
        next_out  = output.data() + used;
        avail_out = output.size() - used;
    }
    JxlEncoderDestroy(enc);

    if (status != JXL_ENC_SUCCESS)
        return "JxlEncoderProcessOutput failed";

    output.resize(static_cast<size_t>(next_out - output.data()));

    FILE* fp = std::fopen(path, "wb");
    if (!fp) return std::string("Cannot open file for writing: ") + path;
    std::fwrite(output.data(), 1, output.size(), fp);
    std::fclose(fp);
    return {};  // success
}
#endif  // HAVE_JXL
