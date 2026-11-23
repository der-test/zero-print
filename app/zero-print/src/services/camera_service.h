#pragma once

#include "raster.h"

#include <string>
#include <vector>

namespace services {

class CameraPreviewStream {
public:
    CameraPreviewStream() = default;
    ~CameraPreviewStream();

    CameraPreviewStream(const CameraPreviewStream&) = delete;
    CameraPreviewStream& operator=(const CameraPreviewStream&) = delete;

    bool start(std::string& error);
    bool read_frame(std::vector<uint8_t>& jpeg_frame, std::string& error);
    bool save_latest_frame(std::string& photo_path, std::string& error);
    void stop();

private:
    int read_fd_ = -1;
    int child_pid_ = -1;
    std::vector<uint8_t> stream_buffer_;
    std::vector<uint8_t> latest_frame_;
};

bool decode_jpeg_grayscale(const uint8_t* jpeg_data,
                           size_t jpeg_size,
                           imaging::GrayImage& image,
                           std::string& error);
bool load_jpeg_grayscale(const std::string& photo_path,
                         imaging::GrayImage& image,
                         std::string& error);

} // namespace services