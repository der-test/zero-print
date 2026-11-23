#include "printer_protocol.h"

#include <array>
#include <limits>

namespace printing {
namespace {

constexpr size_t kMx06Width = 384;
constexpr size_t kMx06RowBytes = 48;
constexpr size_t kMx06MaxRows = 2048;

uint8_t crc8(const uint8_t* data, size_t length) {
    uint8_t crc = 0;
    for (size_t index = 0; index < length; index++) {
        crc ^= data[index];
        for (uint8_t bit = 0; bit < 8; bit++) {
            crc = (crc & 0x80u) != 0 ? static_cast<uint8_t>((crc << 1) ^ 0x07u)
                                     : static_cast<uint8_t>(crc << 1);
        }
    }
    return crc;
}

void append_mx06_command(std::vector<uint8_t>& output,
                         uint8_t command,
                         const uint8_t* payload,
                         uint8_t payload_size) {
    output.push_back(0x51);
    output.push_back(0x78);
    output.push_back(command);
    output.push_back(0x00);
    output.push_back(payload_size);
    output.push_back(0x00);
    output.insert(output.end(), payload, payload + payload_size);
    output.push_back(crc8(payload, payload_size));
    output.push_back(0xff);
}

bool valid_raster(const imaging::RasterImage& raster) {
    return raster.width != 0 && raster.height != 0 && raster.row_bytes != 0 &&
           raster.row_bytes <= std::numeric_limits<size_t>::max() / raster.height &&
           raster.pixels.size() == raster.row_bytes * raster.height &&
           raster.width <= raster.row_bytes * 8;
}

} // namespace

bool make_mx06_job(const imaging::RasterImage& raster,
                   std::vector<uint8_t>& output,
                   std::string& error) {
    if (!valid_raster(raster) || raster.width != kMx06Width ||
        raster.row_bytes != kMx06RowBytes || raster.height > kMx06MaxRows) {
        error = "MX06 requires a 384-pixel raster no taller than 2048 rows";
        return false;
    }

    static constexpr std::array<uint8_t, 11> lattice_start = {
        0xaa, 0x55, 0x17, 0x38, 0x44, 0x5f, 0x5f, 0x5f, 0x44, 0x38, 0x2c
    };
    static constexpr std::array<uint8_t, 11> lattice_end = {
        0xaa, 0x55, 0x17, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x17
    };
    static constexpr std::array<uint8_t, 2> energy = {0x80, 0x3e};
    static constexpr std::array<uint8_t, 1> quality = {0x33};
    static constexpr std::array<uint8_t, 1> drawing_mode = {0x00};
    static constexpr std::array<uint8_t, 2> feed = {50, 0};

    std::vector<uint8_t> job;
    append_mx06_command(job, 0xaf, energy.data(), energy.size());
    append_mx06_command(job, 0xa4, quality.data(), quality.size());
    append_mx06_command(job, 0xbe, drawing_mode.data(), drawing_mode.size());
    append_mx06_command(job, 0xa6, lattice_start.data(), lattice_start.size());

    for (size_t row = 0; row < raster.height; row++) {
        std::array<uint8_t, kMx06RowBytes> payload{};
        const size_t source_row = raster.height - 1 - row;
        for (size_t byte = 0; byte < kMx06RowBytes; byte++) {
            payload[byte] = raster.pixels[source_row * kMx06RowBytes + (kMx06RowBytes - 1 - byte)];
        }
        append_mx06_command(job, 0xa2, payload.data(), payload.size());
    }

    append_mx06_command(job, 0xa6, lattice_end.data(), lattice_end.size());
    append_mx06_command(job, 0xa1, feed.data(), feed.size());
    output = std::move(job);
    error.clear();
    return true;
}

bool make_escpos_job(const imaging::RasterImage& raster,
                     std::vector<uint8_t>& output,
                     std::string& error) {
    if (!valid_raster(raster) || raster.row_bytes > 0xffff || raster.height > 0xffff) {
        error = "ESC/POS raster dimensions are invalid or exceed the command limits";
        return false;
    }

    std::vector<uint8_t> job = {
        0x1b, 0x40,
        0x1d, 0x76, 0x30, 0x00,
        static_cast<uint8_t>(raster.row_bytes & 0xff),
        static_cast<uint8_t>((raster.row_bytes >> 8) & 0xff),
        static_cast<uint8_t>(raster.height & 0xff),
        static_cast<uint8_t>((raster.height >> 8) & 0xff),
    };
    job.insert(job.end(), raster.pixels.begin(), raster.pixels.end());
    job.insert(job.end(), {0x1b, 0x64, 0x03, 0x1d, 0x56, 0x41, 0x00});

    output = std::move(job);
    error.clear();
    return true;
}

} // namespace printing