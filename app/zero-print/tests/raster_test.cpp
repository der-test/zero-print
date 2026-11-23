#include "raster.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

int main() {
    std::string error;
    imaging::GrayImage source{4, 2, {0, 255, 0, 255, 255, 0, 255, 0}};
    imaging::RasterImage raster;

    assert(imaging::dither_to_raster(source,
                                     imaging::DitherMode::Threshold,
                                     128,
                                     raster,
                                     error));
    assert(raster.width == 4);
    assert(raster.height == 2);
    assert(raster.row_bytes == 1);
    assert(raster.pixels == std::vector<uint8_t>({0xa0, 0x50}));

    imaging::GrayImage black_image{3, 1, {0, 0, 0}};
    assert(imaging::dither_to_raster(black_image,
                                     imaging::DitherMode::FloydSteinberg,
                                     128,
                                     raster,
                                     error));
    assert(raster.pixels == std::vector<uint8_t>({0xe0}));

    imaging::GrayImage white_image{3, 1, {255, 255, 255}};
    assert(imaging::dither_to_raster(white_image,
                                     imaging::DitherMode::Atkinson,
                                     128,
                                     raster,
                                     error));
    assert(raster.pixels == std::vector<uint8_t>({0x00}));

    imaging::GrayImage small{2, 1, {0, 255}};
    imaging::GrayImage resized;
    assert(imaging::resize_to_width(small, 4, resized, error));
    assert(resized.width == 4);
    assert(resized.height == 2);
    assert(resized.pixels[0] == 0);
    assert(resized.pixels[1] == 64);
    assert(resized.pixels[2] == 191);
    assert(resized.pixels[3] == 255);

    imaging::GrayImage orientation{2, 2, {1, 2, 3, 4}};
    imaging::GrayImage rotated;
    assert(imaging::rotate_180(orientation, rotated, error));
    assert(rotated.width == 2);
    assert(rotated.height == 2);
    assert(rotated.pixels == std::vector<uint8_t>({4, 3, 2, 1}));

    imaging::GrayImage invalid{2, 2, {0, 255}};
    assert(!imaging::dither_to_raster(invalid,
                                      imaging::DitherMode::Threshold,
                                      128,
                                      raster,
                                      error));
    assert(!error.empty());
    return 0;
}