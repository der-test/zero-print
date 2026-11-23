#include "raster.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace imaging {
namespace {

bool has_valid_pixels(const GrayImage& image) {
    return image.width != 0 && image.height != 0 &&
           image.width <= std::numeric_limits<size_t>::max() / image.height &&
           image.pixels.size() == image.width * image.height;
}

uint8_t sample_bilinear(const GrayImage& image, double x, double y) {
    const double clamped_x = std::clamp(x, 0.0, static_cast<double>(image.width - 1));
    const double clamped_y = std::clamp(y, 0.0, static_cast<double>(image.height - 1));
    const size_t x0 = static_cast<size_t>(clamped_x);
    const size_t y0 = static_cast<size_t>(clamped_y);
    const size_t x1 = std::min(x0 + 1, image.width - 1);
    const size_t y1 = std::min(y0 + 1, image.height - 1);
    const double x_fraction = clamped_x - static_cast<double>(x0);
    const double y_fraction = clamped_y - static_cast<double>(y0);
    const double top = image.pixels[y0 * image.width + x0] * (1.0 - x_fraction) +
                       image.pixels[y0 * image.width + x1] * x_fraction;
    const double bottom = image.pixels[y1 * image.width + x0] * (1.0 - x_fraction) +
                          image.pixels[y1 * image.width + x1] * x_fraction;
    return static_cast<uint8_t>(std::lround(top * (1.0 - y_fraction) + bottom * y_fraction));
}

void add_error(std::vector<int32_t>& pixels,
               size_t width,
               size_t height,
               size_t x,
               size_t y,
               int32_t error,
               int32_t numerator,
               int32_t denominator) {
    if (x >= width || y >= height) {
        return;
    }
    pixels[y * width + x] += (error * numerator) / denominator;
}

} // namespace

bool RasterImage::is_black(size_t x, size_t y) const {
    if (x >= width || y >= height || row_bytes == 0 || pixels.size() < row_bytes * height) {
        return false;
    }
    return (pixels[y * row_bytes + (x / 8)] & (1u << (7u - (x % 8u)))) != 0;
}

bool resize_to_width(const GrayImage& source,
                     size_t target_width,
                     GrayImage& output,
                     std::string& error) {
    if (!has_valid_pixels(source) || target_width == 0) {
        error = "Image dimensions or pixel data are invalid";
        return false;
    }
    if (source.height > std::numeric_limits<size_t>::max() / target_width) {
        error = "Resized image is too large";
        return false;
    }

    const size_t target_height = std::max<size_t>(
        1, static_cast<size_t>(std::lround(static_cast<double>(source.height) * target_width /
                                           source.width)));
    if (target_width > std::numeric_limits<size_t>::max() / target_height) {
        error = "Resized image is too large";
        return false;
    }

    GrayImage resized;
    resized.width = target_width;
    resized.height = target_height;
    resized.pixels.resize(target_width * target_height);

    for (size_t y = 0; y < target_height; y++) {
        const double source_y = (static_cast<double>(y) + 0.5) * source.height / target_height - 0.5;
        for (size_t x = 0; x < target_width; x++) {
            const double source_x = (static_cast<double>(x) + 0.5) * source.width / target_width - 0.5;
            resized.pixels[y * target_width + x] = sample_bilinear(source, source_x, source_y);
        }
    }

    output = std::move(resized);
    error.clear();
    return true;
}

bool rotate_180(const GrayImage& source,
                GrayImage& output,
                std::string& error) {
    if (!has_valid_pixels(source)) {
        error = "Image dimensions or pixel data are invalid";
        return false;
    }

    GrayImage rotated;
    rotated.width = source.width;
    rotated.height = source.height;
    rotated.pixels.resize(source.pixels.size());
    for (size_t y = 0; y < source.height; y++) {
        for (size_t x = 0; x < source.width; x++) {
            const size_t source_index = y * source.width + x;
            const size_t rotated_index = (source.height - 1 - y) * source.width +
                                         (source.width - 1 - x);
            rotated.pixels[rotated_index] = source.pixels[source_index];
        }
    }

    output = std::move(rotated);
    error.clear();
    return true;
}

bool dither_to_raster(const GrayImage& source,
                      DitherMode mode,
                      uint8_t threshold,
                      RasterImage& output,
                      std::string& error) {
    if (!has_valid_pixels(source)) {
        error = "Image dimensions or pixel data are invalid";
        return false;
    }

    const size_t row_bytes = (source.width + 7) / 8;
    if (row_bytes > std::numeric_limits<size_t>::max() / source.height) {
        error = "Raster image is too large";
        return false;
    }

    std::vector<int32_t> working(source.pixels.begin(), source.pixels.end());
    RasterImage raster;
    raster.width = source.width;
    raster.height = source.height;
    raster.row_bytes = row_bytes;
    raster.pixels.assign(row_bytes * source.height, 0);

    for (size_t y = 0; y < source.height; y++) {
        for (size_t x = 0; x < source.width; x++) {
            const size_t index = y * source.width + x;
            const int32_t old_value = std::clamp(working[index], 0, 255);
            const bool black = old_value < threshold;
            const int32_t new_value = black ? 0 : 255;
            if (black) {
                raster.pixels[y * row_bytes + (x / 8)] |=
                    static_cast<uint8_t>(1u << (7u - (x % 8u)));
            }

            const int32_t error_value = old_value - new_value;
            if (mode == DitherMode::FloydSteinberg) {
                add_error(working, source.width, source.height, x + 1, y, error_value, 7, 16);
                if (x > 0) {
                    add_error(working, source.width, source.height, x - 1, y + 1, error_value, 3, 16);
                }
                add_error(working, source.width, source.height, x, y + 1, error_value, 5, 16);
                add_error(working, source.width, source.height, x + 1, y + 1, error_value, 1, 16);
            } else if (mode == DitherMode::Atkinson) {
                const int32_t spread = error_value / 8;
                const auto add_atkinson = [&](size_t target_x, size_t target_y) {
                    if (target_x < source.width && target_y < source.height) {
                        working[target_y * source.width + target_x] += spread;
                    }
                };
                add_atkinson(x + 1, y);
                add_atkinson(x + 2, y);
                if (x > 0) {
                    add_atkinson(x - 1, y + 1);
                }
                add_atkinson(x, y + 1);
                add_atkinson(x + 1, y + 1);
                add_atkinson(x, y + 2);
            }
        }
    }

    output = std::move(raster);
    error.clear();
    return true;
}

} // namespace imaging