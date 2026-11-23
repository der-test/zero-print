/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "application_config.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <filesystem>
#include <fstream>

namespace app {
namespace {

std::string trim(std::string value) {
    const auto is_not_space = [](unsigned char character) {
        return !std::isspace(character);
    };

    value.erase(value.begin(), std::find_if(value.begin(), value.end(), is_not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), is_not_space).base(), value.end());
    return value;
}

std::string lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

bool parse_bool(const std::string& value, bool& result) {
    const auto normalized = lowercase(trim(value));
    if (normalized == "yes" || normalized == "true" || normalized == "1") {
        result = true;
        return true;
    }
    if (normalized == "no" || normalized == "false" || normalized == "0") {
        result = false;
        return true;
    }
    return false;
}

bool parse_integer(const std::string& value, int& result) {
    const auto normalized = trim(value);
    const auto parsed = std::from_chars(normalized.data(),
                                        normalized.data() + normalized.size(),
                                        result);
    return parsed.ec == std::errc{} && parsed.ptr == normalized.data() + normalized.size();
}

} // namespace

bool load_application_config(const std::string& path,
                             ApplicationConfig& config,
                             std::string& error) {
    error.clear();
    std::ifstream input(path);
    if (!input) {
        error = "could not open file";
        return false;
    }

    std::string section;
    std::string line;
    std::size_t line_number = 0;
    while (std::getline(input, line)) {
        ++line_number;
        line = trim(line);
        if (line.empty() || line.front() == '#' || line.front() == ';') {
            continue;
        }

        if (line.front() == '[' && line.back() == ']') {
            section = lowercase(trim(line.substr(1, line.size() - 2)));
            continue;
        }

        const auto separator = line.find('=');
        if (separator == std::string::npos) {
            continue;
        }

        const auto key = lowercase(trim(line.substr(0, separator)));
        const auto value = trim(line.substr(separator + 1));
        if (section == "application" && key == "dark_mode") {
            if (!parse_bool(value, config.dark_mode)) {
                error = "invalid dark_mode value at line " + std::to_string(line_number);
                return false;
            }
        }
        if (section == "printer" && key == "transport") {
            if (!parse_integer(value, config.printer_transport) ||
                config.printer_transport < 0 || config.printer_transport > 1) {
                error = "invalid printer transport at line " + std::to_string(line_number);
                return false;
            }
        }
        if (section == "printer" && key == "dither_mode") {
            if (!parse_integer(value, config.dither_mode) ||
                config.dither_mode < 0 || config.dither_mode > 2) {
                error = "invalid dither mode at line " + std::to_string(line_number);
                return false;
            }
        }
        if (section == "printer" && key == "ble_address") {
            config.ble_address = value;
        }
        if (section == "printer" && key == "usb_device") {
            config.usb_device = value;
        }
    }

    return true;
}

bool save_application_config(const std::string& path,
                             const ApplicationConfig& config,
                             std::string& error) {
    error.clear();

    const std::filesystem::path config_path(path);
    const auto parent = config_path.parent_path();
    if (!parent.empty()) {
        std::error_code filesystem_error;
        std::filesystem::create_directories(parent, filesystem_error);
        if (filesystem_error) {
            error = "could not create parent directory: " + filesystem_error.message();
            return false;
        }
    }

    std::ofstream output(path, std::ios::trunc);
    if (!output) {
        error = "could not open file for writing";
        return false;
    }
    output << "[application]\n"
           << "dark_mode=" << (config.dark_mode ? "yes" : "no") << "\n\n"
           << "[printer]\n"
           << "transport=" << config.printer_transport << '\n'
           << "dither_mode=" << config.dither_mode << '\n'
           << "ble_address=" << config.ble_address << '\n'
           << "usb_device=" << config.usb_device << '\n';
    if (!output) {
        error = "failed while writing file";
        return false;
    }

    return true;
}

} // namespace app
