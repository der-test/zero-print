#include "printer_service.h"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <glob.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

#if !USE_DESKTOP
#include <sys/socket.h>
#include <sys/time.h>
#endif

namespace services {
namespace {

bool write_all(int descriptor, const uint8_t* data, size_t length, std::string& error) {
    size_t offset = 0;
    while (offset < length) {
        const ssize_t written = write(descriptor, data + offset, length - offset);
        if (written < 0) {
            if (errno == EINTR) {
                continue;
            }
            error = std::string("Printer write failed: ") + std::strerror(errno);
            return false;
        }
        if (written == 0) {
            error = "Printer disconnected while writing";
            return false;
        }
        offset += static_cast<size_t>(written);
    }
    return true;
}

#if !USE_DESKTOP
constexpr uint16_t kAttChannelId = 0x0004;
constexpr size_t kBleWriteChunkBytes = 20;
constexpr uint8_t kBleWriteMaxRetries = 8;
constexpr auto kBleWritePacing = std::chrono::milliseconds(1);
constexpr int kBluetoothAddressFamily = 31;
constexpr int kBtpProtocolL2cap = 0;
constexpr int kBluetoothSocketLevel = 274;
constexpr int kBluetoothSecurityOption = 4;
constexpr uint8_t kBluetoothSecurityLow = 1;
constexpr uint8_t kLePublicAddress = 0x01;
constexpr uint8_t kLeRandomAddress = 0x02;

struct BluetoothSecurity {
    uint8_t level;
    uint8_t key_size;
};

struct BluetoothAddress {
    uint8_t bytes[6];
};

struct L2capSocketAddress {
    uint16_t family;
    uint16_t psm;
    BluetoothAddress address;
    uint16_t cid;
    uint8_t address_type;
};

static_assert(sizeof(L2capSocketAddress) == 14);

uint16_t host_to_little_endian(uint16_t value) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return value;
#else
    return __builtin_bswap16(value);
#endif
}

bool run_command(const std::vector<std::string>& arguments,
                 std::string& output,
                 int& exit_code) {
    int pipe_fds[2];
    if (pipe(pipe_fds) != 0) {
        return false;
    }

    const pid_t child = fork();
    if (child < 0) {
        close(pipe_fds[0]);
        close(pipe_fds[1]);
        return false;
    }
    if (child == 0) {
        close(pipe_fds[0]);
        dup2(pipe_fds[1], STDOUT_FILENO);
        dup2(pipe_fds[1], STDERR_FILENO);
        close(pipe_fds[1]);
        std::vector<char*> argv;
        argv.reserve(arguments.size() + 1);
        for (const auto& argument : arguments) {
            argv.push_back(const_cast<char*>(argument.c_str()));
        }
        argv.push_back(nullptr);
        execvp(argv[0], argv.data());
        _exit(errno == ENOENT ? 127 : 126);
    }

    close(pipe_fds[1]);
    std::array<char, 512> buffer{};
    ssize_t count = 0;
    while ((count = read(pipe_fds[0], buffer.data(), buffer.size())) > 0) {
        output.append(buffer.data(), static_cast<size_t>(count));
    }
    close(pipe_fds[0]);

    int status = 0;
    while (waitpid(child, &status, 0) < 0) {
        if (errno != EINTR) {
            return false;
        }
    }
    exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : 128;
    return true;
}

bool parse_bluetooth_address(const std::string& text, BluetoothAddress& address) {
    if (text.size() != 17) {
        return false;
    }
    for (size_t byte = 0; byte < 6; byte++) {
        const size_t position = byte * 3;
        char* end = nullptr;
        const unsigned long value = std::strtoul(text.c_str() + position, &end, 16);
        if (!end || end != text.c_str() + position + 2 || value > 0xff ||
            (byte < 5 && text[position + 2] != ':')) {
            return false;
        }
        address.bytes[5 - byte] = static_cast<uint8_t>(value);
    }
    return true;
}

int connect_att(const std::string& text, std::string& error) {
    BluetoothAddress bluetooth_address{};
    if (!parse_bluetooth_address(text, bluetooth_address)) {
        error = "The selected BLE address is invalid";
        return -1;
    }

    int last_connect_error = 0;
    for (uint8_t address_type : {kLePublicAddress, kLeRandomAddress}) {
        const int socket_fd = socket(kBluetoothAddressFamily, SOCK_SEQPACKET, kBtpProtocolL2cap);
        if (socket_fd < 0) {
            error = std::string("Unable to open Bluetooth: ") + std::strerror(errno);
            return -1;
        }

        timeval timeout{3, 0};
        setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        setsockopt(socket_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

        L2capSocketAddress local{};
        local.family = static_cast<uint16_t>(kBluetoothAddressFamily);
        local.cid = host_to_little_endian(kAttChannelId);
        if (bind(socket_fd, reinterpret_cast<sockaddr*>(&local), sizeof(local)) != 0) {
            const int bind_error = errno;
            close(socket_fd);
            error = std::string("Unable to bind local BLE ATT socket: ") + std::strerror(bind_error);
            return -1;
        }

        const BluetoothSecurity security{kBluetoothSecurityLow, 0};
        if (setsockopt(socket_fd,
                       kBluetoothSocketLevel,
                       kBluetoothSecurityOption,
                       &security,
                       sizeof(security)) != 0) {
            const int security_error = errno;
            close(socket_fd);
            error = std::string("Unable to set BLE security: ") + std::strerror(security_error);
            return -1;
        }

        L2capSocketAddress peer{};
        peer.family = static_cast<uint16_t>(kBluetoothAddressFamily);
        peer.cid = host_to_little_endian(kAttChannelId);
        peer.address_type = address_type;
        peer.address = bluetooth_address;
        if (connect(socket_fd, reinterpret_cast<sockaddr*>(&peer), sizeof(peer)) == 0) {
            error.clear();
            return socket_fd;
        }
        last_connect_error = errno;
        close(socket_fd);
    }

    error = std::string("Unable to connect to the MX06: ") + std::strerror(last_connect_error);
    return -1;
}

bool att_request(int socket_fd,
                 const std::vector<uint8_t>& request,
                 std::array<uint8_t, 517>& response,
                 size_t& response_size,
                 std::string& error) {
    const ssize_t sent = send(socket_fd, request.data(), request.size(), 0);
    if (sent != static_cast<ssize_t>(request.size())) {
        error = std::string("BLE request failed: ") + std::strerror(errno);
        return false;
    }

    const ssize_t received = recv(socket_fd, response.data(), response.size(), 0);
    if (received < 0) {
        error = std::string("BLE response timed out: ") + std::strerror(errno);
        return false;
    }
    response_size = static_cast<size_t>(received);
    if (response_size >= 5 && response[0] == 0x01) {
        error = "The MX06 rejected a BLE discovery request";
        return false;
    }
    return response_size > 0;
}

bool discover_handles(int socket_fd,
                      uint16_t& data_handle,
                      std::string& error) {
    uint16_t service_start = 1;
    uint16_t service_end = 0xffff;
    std::array<uint8_t, 517> response{};
    size_t response_size = 0;
    bool service_found = false;

    while (service_start <= service_end) {
        const std::vector<uint8_t> request = {
            0x10,
            static_cast<uint8_t>(service_start & 0xff),
            static_cast<uint8_t>(service_start >> 8),
            static_cast<uint8_t>(service_end & 0xff),
            static_cast<uint8_t>(service_end >> 8),
            0x00,
            0x28,
        };
        if (!att_request(socket_fd, request, response, response_size, error)) {
            return false;
        }
        if (response[0] != 0x11 || response_size < 2 || response[1] < 6) {
            break;
        }

        const size_t entry_size = response[1];
        uint16_t next_start = 0;
        for (size_t offset = 2; offset + entry_size <= response_size; offset += entry_size) {
            const uint16_t start = static_cast<uint16_t>(response[offset] | (response[offset + 1] << 8));
            const uint16_t end = static_cast<uint16_t>(response[offset + 2] | (response[offset + 3] << 8));
            const uint16_t uuid = static_cast<uint16_t>(response[offset + 4] | (response[offset + 5] << 8));
            if (uuid == 0xae30) {
                service_start = start;
                service_end = end;
                service_found = true;
                break;
            }
            next_start = static_cast<uint16_t>(end + 1);
        }
        if (service_found) {
            break;
        }
        if (next_start == 0 || next_start <= service_start) {
            break;
        }
        service_start = next_start;
    }

    if (!service_found) {
        error = "The MX06 BLE service AE30 was not found";
        return false;
    }

    uint16_t next_handle = service_start;
    while (next_handle <= service_end) {
        const std::vector<uint8_t> request = {
            0x08,
            static_cast<uint8_t>(next_handle & 0xff),
            static_cast<uint8_t>(next_handle >> 8),
            static_cast<uint8_t>(service_end & 0xff),
            static_cast<uint8_t>(service_end >> 8),
            0x03,
            0x28,
        };
        if (!att_request(socket_fd, request, response, response_size, error)) {
            return false;
        }
        if (response[0] != 0x09 || response_size < 2 || response[1] < 7) {
            break;
        }

        const size_t entry_size = response[1];
        uint16_t last_declaration = 0;
        for (size_t offset = 2; offset + entry_size <= response_size; offset += entry_size) {
            last_declaration = static_cast<uint16_t>(response[offset] | (response[offset + 1] << 8));
            const uint16_t value_handle = static_cast<uint16_t>(response[offset + 3] |
                                                                (response[offset + 4] << 8));
            const uint16_t uuid = static_cast<uint16_t>(response[offset + 5] |
                                                        (response[offset + 6] << 8));
            if (uuid == 0xae01) {
                data_handle = value_handle;
                return true;
            }
        }
        if (last_declaration == 0 || last_declaration == 0xffff) {
            break;
        }
        next_handle = static_cast<uint16_t>(last_declaration + 1);
    }

    error = "The MX06 BLE write characteristic AE01 was not found";
    return false;
}

bool send_ble_job(std::string& address,
                  const std::vector<uint8_t>& job,
                  std::string& error) {
    int socket_fd = connect_att(address, error);
    if (socket_fd < 0) {
        const std::string initial_error = error;
        std::string refreshed_address;
        std::string scan_error;
        if (scan_mx06(refreshed_address, scan_error)) {
            address = refreshed_address;
        } else {
            refreshed_address = address;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        socket_fd = connect_att(address, error);
        if (socket_fd < 0) {
            error = initial_error + "; MX06 rescan " +
                    (scan_error.empty() ? "succeeded" : "failed: " + scan_error) +
                    "; reconnect failed: " + error;
            return false;
        }
    }

    uint16_t data_handle = 0;
    if (!discover_handles(socket_fd, data_handle, error)) {
        close(socket_fd);
        return false;
    }

    size_t offset = 0;
    while (offset < job.size()) {
        const size_t chunk_size = std::min(kBleWriteChunkBytes, job.size() - offset);
        std::vector<uint8_t> command = {
            0x52,
            static_cast<uint8_t>(data_handle & 0xff),
            static_cast<uint8_t>(data_handle >> 8),
        };
        command.insert(command.end(), job.begin() + offset, job.begin() + offset + chunk_size);

        uint8_t retry = 0;
        for (;;) {
            const ssize_t sent = send(socket_fd, command.data(), command.size(), 0);
            if (sent == static_cast<ssize_t>(command.size())) {
                break;
            }
            if (sent < 0 && errno == EINTR) {
                continue;
            }
            if (sent < 0 && retry < kBleWriteMaxRetries &&
                (errno == EAGAIN || errno == EWOULDBLOCK || errno == ENOBUFS || errno == ENOMEM)) {
                retry++;
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                continue;
            }
            const int write_error = sent < 0 ? errno : EIO;
            error = std::string("BLE printer write failed: ") + std::strerror(write_error);
            close(socket_fd);
            return false;
        }
        offset += chunk_size;
        std::this_thread::sleep_for(kBleWritePacing);
    }

    close(socket_fd);
    error.clear();
    return true;
}
#endif

} // namespace

bool scan_mx06(std::string& address, std::string& error) {
#if !USE_DESKTOP
    std::string output;
    int exit_code = 0;
    if (!run_command({"bluetoothctl", "--timeout", "8", "scan", "on"}, output, exit_code)) {
        error = "Unable to start Bluetooth discovery";
        return false;
    }
    if (exit_code == 127) {
        error = "bluetoothctl is not installed";
        return false;
    }

    size_t line_start = 0;
    while (line_start < output.size()) {
        const size_t line_end = output.find('\n', line_start);
        const std::string line = output.substr(line_start,
                                               line_end == std::string::npos
                                                   ? std::string::npos
                                                   : line_end - line_start);
        if (line.find("MX06") != std::string::npos) {
            const size_t device_start = line.find("Device ");
            if (device_start != std::string::npos && device_start + 24 <= line.size()) {
                address = line.substr(device_start + 7, 17);
                error.clear();
                return true;
            }
        }
        if (line_end == std::string::npos) {
            break;
        }
        line_start = line_end + 1;
    }

    error = "No MX06 printer was found during the Bluetooth scan";
    return false;
#else
    (void)address;
    error = "Bluetooth scanning is available on CardputerZero only";
    return false;
#endif
}

std::vector<std::string> scan_usb_printers() {
    std::vector<std::string> devices;
    glob_t matches{};
    if (glob("/dev/usb/lp*", 0, nullptr, &matches) == 0) {
        for (size_t index = 0; index < matches.gl_pathc; index++) {
            devices.emplace_back(matches.gl_pathv[index]);
        }
    }
    globfree(&matches);
    return devices;
}

bool print_raster(const imaging::RasterImage& raster,
                  PrinterConfig& config,
                  std::string& error) {
    std::vector<uint8_t> job;
    const bool encoded = config.transport == PrinterTransport::BleMx06
                             ? printing::make_mx06_job(raster, job, error)
                             : printing::make_escpos_job(raster, job, error);
    if (!encoded) {
        return false;
    }

    if (config.transport == PrinterTransport::BleMx06) {
#if !USE_DESKTOP
        if (config.ble_address.empty()) {
            error = "Scan for an MX06 printer in Settings first";
            return false;
        }
        return send_ble_job(config.ble_address, job, error);
#else
        error = "Bluetooth printing is available on CardputerZero only";
        return false;
#endif
    }

    if (config.usb_device.empty()) {
        error = "Choose a USB printer device in Settings first";
        return false;
    }
    const int printer = open(config.usb_device.c_str(), O_WRONLY | O_CLOEXEC);
    if (printer < 0) {
        error = std::string("Unable to open ") + config.usb_device + ": " + std::strerror(errno);
        return false;
    }
    const bool result = write_all(printer, job.data(), job.size(), error);
    close(printer);
    return result;
}

} // namespace services