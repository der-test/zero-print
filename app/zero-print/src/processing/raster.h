#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace imaging {

enum class DitherMode : uint8_t {
    Threshold,
    FloydSteinberg,
    Atkinson,
};

struct GrayImage {
    size_t width = 0;
    size_t height = 0;
    std::vector<uint8_t> pixels;
};

struct RasterImage {
    size_t width = 0;
    size_t height = 0;
    size_t row_bytes = 0;
    std::vector<uint8_t> pixels;

    bool is_black(size_t x, size_t y) const;
};

bool resize_to_width(const GrayImage& source,
                     size_t target_width,
                     GrayImage& output,
                     std::string& error);

bool rotate_180(const GrayImage& source,
                GrayImage& output,
                std::string& error);

bool dither_to_raster(const GrayImage& source,
                      DitherMode mode,
                      uint8_t threshold,
                      RasterImage& output,
                      std::string& error);

} // namespace imaging