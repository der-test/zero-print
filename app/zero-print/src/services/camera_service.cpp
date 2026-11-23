#include "camera_service.h"

#include <cerrno>
#include <algorithm>
#include <array>
#include <fcntl.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <jpeglib.h>
#include <iterator>
#include <poll.h>
#include <setjmp.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#include <limits>
#include <chrono>
#include <string>
#include <vector>

extern "C" char** environ;

namespace services {
namespace {

struct JpegErrorContext {
    jpeg_error_mgr manager;
    jmp_buf jump;
    char message[JMSG_LENGTH_MAX];
    uint8_t* pixels;
};

void jpeg_error_exit(j_common_ptr common) {
    auto* context = reinterpret_cast<JpegErrorContext*>(common->err);
    (*common->err->format_message)(common, context->message);
    longjmp(context->jump, 1);
}

#if !USE_DESKTOP
int spawn_preview_command(const char* executable,
                          int read_fd,
                          int write_fd,
                          pid_t& child) {
    std::array<char*, 15> argv = {
        const_cast<char*>(executable),
        const_cast<char*>("--nopreview"),
        const_cast<char*>("--timeout"),
        const_cast<char*>("0"),
        const_cast<char*>("--codec"),
        const_cast<char*>("mjpeg"),
        const_cast<char*>("--width"),
        const_cast<char*>("640"),
        const_cast<char*>("--height"),
        const_cast<char*>("480"),
        const_cast<char*>("--framerate"),
        const_cast<char*>("10"),
        const_cast<char*>("--output"),
        const_cast<char*>("-"),
        nullptr,
    };

    posix_spawn_file_actions_t actions;
    int result = posix_spawn_file_actions_init(&actions);
    if (result != 0) {
        return result;
    }
    result = posix_spawn_file_actions_adddup2(&actions, write_fd, STDOUT_FILENO);
    if (result == 0) {
        result = posix_spawn_file_actions_addclose(&actions, read_fd);
    }
    if (result == 0) {
        result = posix_spawn_file_actions_addclose(&actions, write_fd);
    }
    if (result == 0) {
        result = posix_spawnp(&child, executable, &actions, nullptr, argv.data(), environ);
    }
    posix_spawn_file_actions_destroy(&actions);
    return result;
}
#endif

} // namespace

CameraPreviewStream::~CameraPreviewStream() {
    stop();
}

bool CameraPreviewStream::start(std::string& error) {
    stop();
#if USE_DESKTOP
    error = "Live camera preview is available on CardputerZero only";
    return false;
#else
    int pipe_fds[2];
    if (pipe(pipe_fds) != 0) {
        error = std::string("Unable to create camera stream: ") + std::strerror(errno);
        return false;
    }

    pid_t child = -1;
    int spawn_error = ENOENT;
    const char* commands[] = {"rpicam-vid", "libcamera-vid"};
    for (const char* command : commands) {
        spawn_error = spawn_preview_command(command, pipe_fds[0], pipe_fds[1], child);
        if (spawn_error == 0 || spawn_error != ENOENT) {
            break;
        }
    }
    close(pipe_fds[1]);
    if (spawn_error != 0) {
        close(pipe_fds[0]);
        error = spawn_error == ENOENT
                    ? "No rpicam-vid or libcamera-vid command is installed"
                    : std::string("Unable to start camera preview: ") + std::strerror(spawn_error);
        return false;
    }

    const int flags = fcntl(pipe_fds[0], F_GETFL, 0);
    if (flags < 0 || fcntl(pipe_fds[0], F_SETFL, flags | O_NONBLOCK) < 0) {
        error = std::string("Unable to configure camera stream: ") + std::strerror(errno);
        close(pipe_fds[0]);
        kill(child, SIGTERM);
        waitpid(child, nullptr, 0);
        return false;
    }

    read_fd_ = pipe_fds[0];
    child_pid_ = static_cast<int>(child);
    stream_buffer_.clear();
    error.clear();
    return true;
#endif
}

bool CameraPreviewStream::read_frame(std::vector<uint8_t>& jpeg_frame, std::string& error) {
    jpeg_frame.clear();
    error.clear();
#if USE_DESKTOP
    return false;
#else
    if (read_fd_ < 0) {
        return false;
    }

    std::array<uint8_t, 8192> bytes{};
    for (;;) {
        const ssize_t count = read(read_fd_, bytes.data(), bytes.size());
        if (count > 0) {
            stream_buffer_.insert(stream_buffer_.end(), bytes.begin(), bytes.begin() + count);
            continue;
        }
        if (count == 0) {
            error = "Camera preview stream ended";
            stop();
            return false;
        }
        if (errno == EINTR) {
            continue;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            break;
        }
        error = std::string("Camera preview read failed: ") + std::strerror(errno);
        stop();
        return false;
    }

    static constexpr std::array<uint8_t, 2> start_marker = {0xff, 0xd8};
    static constexpr std::array<uint8_t, 2> end_marker = {0xff, 0xd9};
    bool found_frame = false;
    for (;;) {
        auto start = std::search(stream_buffer_.begin(), stream_buffer_.end(),
                                 start_marker.begin(), start_marker.end());
        if (start == stream_buffer_.end()) {
            const bool keep_marker_prefix = !stream_buffer_.empty() && stream_buffer_.back() == 0xff;
            stream_buffer_.clear();
            if (keep_marker_prefix) {
                stream_buffer_.push_back(0xff);
            }
            break;
        }
        if (start != stream_buffer_.begin()) {
            stream_buffer_.erase(stream_buffer_.begin(), start);
        }

        auto end = std::search(stream_buffer_.begin() + 2, stream_buffer_.end(),
                               end_marker.begin(), end_marker.end());
        if (end == stream_buffer_.end()) {
            if (stream_buffer_.size() > 1024U * 1024U) {
                stream_buffer_.clear();
                error = "Camera preview frame exceeded the size limit";
            }
            break;
        }

        jpeg_frame.assign(stream_buffer_.begin(), end + 2);
        stream_buffer_.erase(stream_buffer_.begin(), end + 2);
        found_frame = true;
    }
    if (found_frame) {
        latest_frame_ = jpeg_frame;
    }
    return found_frame;
#endif
}

void CameraPreviewStream::stop() {
    if (read_fd_ >= 0) {
        close(read_fd_);
        read_fd_ = -1;
    }
    if (child_pid_ > 0) {
        const pid_t child = static_cast<pid_t>(child_pid_);
        kill(child, SIGTERM);
        while (waitpid(child, nullptr, 0) < 0 && errno == EINTR) {
        }
        child_pid_ = -1;
    }
    stream_buffer_.clear();
    latest_frame_.clear();
}

bool CameraPreviewStream::save_latest_frame(std::string& photo_path, std::string& error) {
#if USE_DESKTOP
    error = "Photo capture is available on CardputerZero only";
    return false;
#else
    if (latest_frame_.empty()) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(600);
        while (latest_frame_.empty()) {
            std::vector<uint8_t> first_frame;
            if (read_frame(first_frame, error)) {
                continue;
            }
            if (!error.empty()) {
                return false;
            }
            if (read_fd_ < 0) {
                error = "Camera preview stream is not running";
                return false;
            }

            const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                deadline - std::chrono::steady_clock::now());
            if (remaining.count() <= 0) {
                error = "Camera preview did not produce a frame in time";
                return false;
            }

            pollfd stream_fd{read_fd_, POLLIN | POLLERR | POLLHUP, 0};
            const int poll_result = poll(&stream_fd, 1, static_cast<int>(remaining.count()));
            if (poll_result < 0 && errno != EINTR) {
                error = std::string("Camera preview wait failed: ") + std::strerror(errno);
                return false;
            }
        }
    }

    char path_template[] = "/tmp/zero-print-XXXXXX.jpg";
    const int file = mkstemps(path_template, 4);
    if (file < 0) {
        error = "Unable to create a temporary photo file";
        return false;
    }
    size_t offset = 0;
    while (offset < latest_frame_.size()) {
        const ssize_t written = write(file,
                                      latest_frame_.data() + offset,
                                      latest_frame_.size() - offset);
        if (written < 0) {
            if (errno == EINTR) {
                continue;
            }
            const int write_error = errno;
            close(file);
            unlink(path_template);
            error = std::string("Unable to save captured frame: ") + std::strerror(write_error);
            return false;
        }
        if (written == 0) {
            close(file);
            unlink(path_template);
            error = "Unable to save captured frame";
            return false;
        }
        offset += static_cast<size_t>(written);
    }
    if (close(file) != 0) {
        unlink(path_template);
        error = std::string("Unable to finish captured frame: ") + std::strerror(errno);
        return false;
    }

    photo_path = path_template;
    error.clear();
    return true;
#endif
}

bool decode_jpeg_grayscale(const uint8_t* jpeg_data,
                           size_t jpeg_size,
                           imaging::GrayImage& image,
                           std::string& error) {
    if (!jpeg_data || jpeg_size == 0 || jpeg_size > std::numeric_limits<unsigned long>::max()) {
        error = "JPEG data is empty or too large";
        return false;
    }

    auto* decoder = static_cast<jpeg_decompress_struct*>(std::calloc(1, sizeof(jpeg_decompress_struct)));
    auto* context = static_cast<JpegErrorContext*>(std::calloc(1, sizeof(JpegErrorContext)));
    if (!decoder || !context) {
        std::free(decoder);
        std::free(context);
        error = "Not enough memory to decode the photo";
        return false;
    }

    decoder->err = jpeg_std_error(&context->manager);
    context->manager.error_exit = jpeg_error_exit;
    volatile bool decoder_created = false;
    if (setjmp(context->jump)) {
        if (decoder_created) {
            jpeg_destroy_decompress(decoder);
        }
        error = context->message[0] ? context->message : "Invalid JPEG photo";
        std::free(context->pixels);
        std::free(context);
        std::free(decoder);
        return false;
    }

    jpeg_create_decompress(decoder);
    decoder_created = true;
    jpeg_mem_src(decoder, jpeg_data, static_cast<unsigned long>(jpeg_size));
    if (jpeg_read_header(decoder, TRUE) != JPEG_HEADER_OK) {
        jpeg_destroy_decompress(decoder);
        std::free(context);
        std::free(decoder);
        error = "Invalid JPEG photo";
        return false;
    }

    decoder->out_color_space = JCS_GRAYSCALE;
    jpeg_start_decompress(decoder);
    const size_t width = decoder->output_width;
    const size_t height = decoder->output_height;
    if (width == 0 || height == 0 || width > 8192 || height > 8192 ||
        width > std::numeric_limits<size_t>::max() / height || width * height > 16U * 1024U * 1024U) {
        jpeg_destroy_decompress(decoder);
        std::free(context);
        std::free(decoder);
        error = "Photo dimensions exceed the supported limit";
        return false;
    }

    context->pixels = static_cast<uint8_t*>(std::malloc(width * height));
    if (!context->pixels) {
        jpeg_destroy_decompress(decoder);
        std::free(context);
        std::free(decoder);
        error = "Not enough memory to decode the photo";
        return false;
    }

    while (decoder->output_scanline < decoder->output_height) {
        JSAMPROW row = context->pixels + decoder->output_scanline * width;
        if (jpeg_read_scanlines(decoder, &row, 1) != 1) {
            longjmp(context->jump, 1);
        }
    }
    jpeg_finish_decompress(decoder);
    jpeg_destroy_decompress(decoder);
    decoder_created = false;

    imaging::GrayImage decoded;
    decoded.width = width;
    decoded.height = height;
    decoded.pixels.assign(context->pixels, context->pixels + (width * height));
    std::free(context->pixels);
    std::free(context);
    std::free(decoder);
    image = std::move(decoded);
    error.clear();
    return true;
}

bool load_jpeg_grayscale(const std::string& photo_path,
                         imaging::GrayImage& image,
                         std::string& error) {
    std::ifstream input(photo_path, std::ios::binary);
    if (!input) {
        error = "Unable to open the captured photo";
        return false;
    }
    const std::vector<uint8_t> encoded((std::istreambuf_iterator<char>(input)),
                                       std::istreambuf_iterator<char>());
    return decode_jpeg_grayscale(encoded.data(), encoded.size(), image, error);
}

} // namespace services