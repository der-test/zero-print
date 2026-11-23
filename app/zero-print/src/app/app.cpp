/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "app.h"

#include "application_config.h"
#include "asset_manager.h"
#include "camera_service.h"
#include "logger.h"
#include "linux_input.h"
#include "printer_service.h"
#include "raster.h"
#include "screen_manager.h"
#include "base_viewmodel.h"
#include "theme.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <system_error>
#include <utility>

#if USE_DESKTOP
#include "desktop_simulator_frame.h"
#endif

#if !USE_DESKTOP
#if APP_USE_DRM
#include "src/drivers/display/drm/lv_linux_drm.h"
#else
#include "src/drivers/display/fb/lv_linux_fbdev.h"
#endif
#endif

#ifndef APP_FRAMEBUFFER_DEVICE
#define APP_FRAMEBUFFER_DEVICE "/dev/fb0"
#endif

#ifndef APP_DRM_DEVICE
#define APP_DRM_DEVICE "/dev/dri/card0"
#endif

#ifndef APP_DRM_CONNECTOR_ID
#define APP_DRM_CONNECTOR_ID -1
#endif

#ifndef APP_CONFIG_FILE
#define APP_CONFIG_FILE "zero-print.conf"
#endif

namespace app {
namespace {

void quit_requested_observer(lv_observer_t* observer, lv_subject_t* subject) {
    auto* running = static_cast<bool*>(lv_observer_get_user_data(observer));
    if (running && lv_subject_get_int(subject)) {
        *running = false;
    }
}

struct ConfigPersistence {
    std::string config_path;
    bool last_dark_mode;
    viewmodel::BaseViewModel* view_model;
};

std::string writable_config_path() {
#if USE_DESKTOP
    return APP_CONFIG_FILE;
#else
    if (const char* xdg_config_home = std::getenv("XDG_CONFIG_HOME")) {
        const std::filesystem::path root(xdg_config_home);
        if (!root.empty() && root.is_absolute()) {
            return (root / "zero-print" / "zero-print.conf").string();
        }
    }
    if (const char* home = std::getenv("HOME")) {
        const std::filesystem::path root(home);
        if (!root.empty() && root.is_absolute()) {
            return (root / ".config" / "zero-print" / "zero-print.conf").string();
        }
    }
    return APP_CONFIG_FILE;
#endif
}

ApplicationConfig make_config(const viewmodel::BaseViewModel& view_model) {
    ApplicationConfig config;
    config.dark_mode = view_model.is_dark_mode();
    config.printer_transport = view_model.printer_transport();
    config.dither_mode = view_model.dither_mode();
    config.ble_address = view_model.ble_address();
    config.usb_device = view_model.usb_device();
    return config;
}

bool persist_config(const std::string& path, const viewmodel::BaseViewModel& view_model) {
    std::string error;
    if (save_application_config(path, make_config(view_model), error)) {
        return true;
    }
    LOG_WARN("failed to save config {}: {}", path, error);
    return false;
}

void persist_config_observer(lv_observer_t* observer, lv_subject_t* subject) {
    auto* persistence = static_cast<ConfigPersistence*>(lv_observer_get_user_data(observer));
    if (!persistence || !persistence->view_model) {
        return;
    }
    const bool dark_mode = lv_subject_get_int(subject) != 0;
    if (dark_mode == persistence->last_dark_mode) {
        return;
    }
    persistence->last_dark_mode = dark_mode;
    persist_config(persistence->config_path, *persistence->view_model);
}

class ShortcutController {
public:
    ShortcutController(viewmodel::BaseViewModel& view_model,
                       std::string config_path,
                       services::CameraPreviewStream& camera_preview)
        : view_model_(view_model), config_path_(std::move(config_path)), camera_preview_(camera_preview) {
        platform::set_global_key_listener(global_key_listener, this);
        platform::set_key_release_listener(key_release_listener, this);
    }

    ~ShortcutController() {
        platform::clear_key_release_listener(key_release_listener, this);
        platform::clear_global_key_listener(global_key_listener, this);
        camera_preview_.stop();
        if (!view_model_.photo_path().empty()) {
            std::error_code error;
            std::filesystem::remove(view_model_.photo_path(), error);
        }
    }

private:
    bool handle_key(uint32_t key, bool long_pressed) {
        if (key == LV_KEY_ESC) {
            if (long_pressed) {
                escape_long_pressed_ = true;
                view_model_.request_quit();
            } else {
                escape_long_pressed_ = false;
            }
            return true;
        }
        if (long_pressed) {
            return false;
        }

        switch (key) {
            case LV_KEY_ENTER:
                activate_current_page();
                return true;
            case LV_KEY_UP:
                move_selection(key, -1);
                return true;
            case LV_KEY_DOWN:
                move_selection(key, 1);
                return true;
            case LV_KEY_LEFT:
                move_selection(key, -1);
                return true;
            case LV_KEY_RIGHT:
                move_selection(key, 1);
                return true;
            default:
                return false;
        }
    }

    void move_selection(uint32_t key, int delta) {
        switch (view_model_.current_page()) {
            case model::AppPage::Home:
                view_model_.move_home_selection(delta);
                break;
            case model::AppPage::Settings:
                if (key == LV_KEY_LEFT || key == LV_KEY_RIGHT) {
                    if (view_model_.settings_row() == 1) {
                        scan_printer();
                    } else {
                        cycle_setting(delta);
                    }
                } else {
                    view_model_.move_settings_row(delta);
                }
                break;
            default:
                break;
        }
    }

    void cycle_setting(int delta) {
        if (view_model_.settings_row() == 0) {
            view_model_.cycle_printer_transport(delta);
        } else if (view_model_.settings_row() == 2) {
            view_model_.cycle_dither_mode(delta);
        }
        persist_config(config_path_, view_model_);
    }

    void activate_current_page() {
        switch (view_model_.current_page()) {
            case model::AppPage::Home:
                if (view_model_.home_selection() == 0) {
                    view_model_.set_status("Ready to capture");
                    view_model_.show_camera_page();
                } else {
                    view_model_.show_settings_page();
                }
                break;
            case model::AppPage::Settings:
                if (view_model_.settings_row() < 2) {
                    scan_printer();
                } else {
                    view_model_.cycle_dither_mode(1);
                    persist_config(config_path_, view_model_);
                }
                break;
            case model::AppPage::Camera:
                capture_photo();
                break;
            case model::AppPage::Preview:
                print_photo();
                break;
            default:
                break;
        }
    }

    void scan_printer() {
        if (view_model_.printer_transport() == 0) {
            view_model_.set_status("Searching for MX06");
            std::string address;
            std::string error;
            if (!services::scan_mx06(address, error)) {
                view_model_.set_status(error);
                return;
            }
            view_model_.set_ble_address(address);
            view_model_.set_status("MX06 ready");
        } else {
            const auto devices = services::scan_usb_printers();
            if (devices.empty()) {
                view_model_.set_usb_device("");
                view_model_.set_status("No USB printer found");
                return;
            }
            view_model_.set_usb_device(devices.front());
            view_model_.set_status("USB printer ready");
        }
        persist_config(config_path_, view_model_);
    }

    void handle_release(uint32_t key) {
        if (key != LV_KEY_ESC) {
            return;
        }
        if (!escape_long_pressed_) {
            view_model_.back();
        }
        escape_long_pressed_ = false;
    }

    void capture_photo() {
        view_model_.set_status("Saving current frame");
        std::string path;
        std::string error;
        if (!camera_preview_.save_latest_frame(path, error)) {
            std::error_code filesystem_error;
            std::filesystem::remove(path, filesystem_error);
            view_model_.set_status(error);
            return;
        }
        if (!view_model_.photo_path().empty()) {
            std::error_code filesystem_error;
            std::filesystem::remove(view_model_.photo_path(), filesystem_error);
        }
        view_model_.set_photo_path(path);
        view_model_.set_status(view_model_.printer_transport() == 0
                       ? "Captured - MX06 BLE"
                       : "Captured - USB ESC/POS");
        view_model_.show_preview_page();
    }

    void print_photo() {
        if (view_model_.photo_path().empty()) {
            view_model_.set_status("Capture a photo first");
            return;
        }

        const auto print_started = std::chrono::steady_clock::now();
        view_model_.set_status("Preparing raster");
        imaging::GrayImage source;
        imaging::GrayImage oriented;
        imaging::GrayImage resized;
        imaging::RasterImage raster;
        std::string error;
        if (!services::load_jpeg_grayscale(view_model_.photo_path(), source, error)) {
            view_model_.set_status(error);
            return;
        }

        const bool use_ble = view_model_.printer_transport() == 0;
        const size_t width = use_ble ? 384 : 576;
        const imaging::DitherMode dither = view_model_.dither_mode() == 0
                                               ? imaging::DitherMode::FloydSteinberg
                                               : view_model_.dither_mode() == 1
                                                     ? imaging::DitherMode::Atkinson
                                                     : imaging::DitherMode::Threshold;
        if (!imaging::rotate_180(source, oriented, error) ||
            !imaging::resize_to_width(oriented, width, resized, error) ||
            !imaging::dither_to_raster(resized, dither, 128, raster, error)) {
            view_model_.set_status(error);
            return;
        }

        services::PrinterConfig printer;
        printer.transport = use_ble ? services::PrinterTransport::BleMx06
                                    : services::PrinterTransport::UsbEscpos;
        printer.ble_address = view_model_.ble_address();
        printer.usb_device = view_model_.usb_device();
        view_model_.set_status("Printing photo");
        const std::string configured_ble_address = view_model_.ble_address();
        if (!services::print_raster(raster, printer, error)) {
            if (printer.transport == services::PrinterTransport::BleMx06 &&
                printer.ble_address != configured_ble_address) {
                view_model_.set_ble_address(printer.ble_address);
                persist_config(config_path_, view_model_);
            }
            view_model_.set_status(error);
            return;
        }
        if (printer.transport == services::PrinterTransport::BleMx06 &&
            printer.ble_address != configured_ble_address) {
            view_model_.set_ble_address(printer.ble_address);
            persist_config(config_path_, view_model_);
        }
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - print_started);
        LOG_INFO("printed photo in {} ms", elapsed.count());
        view_model_.set_status("Printed in " + std::to_string(elapsed.count()) + " ms");
    }

    static bool global_key_listener(uint32_t key,
                                    const char*,
                                    bool long_pressed,
                                    void* user_data) {
        auto* controller = static_cast<ShortcutController*>(user_data);
        return controller && controller->handle_key(key, long_pressed);
    }

    static void key_release_listener(uint32_t key, const char*, void* user_data) {
        auto* controller = static_cast<ShortcutController*>(user_data);
        if (controller) {
            controller->handle_release(key);
        }
    }

    viewmodel::BaseViewModel& view_model_;
    std::string config_path_;
    services::CameraPreviewStream& camera_preview_;
    bool escape_long_pressed_{false};
};

#if !USE_DESKTOP
lv_display_t* init_device_display() {
#if APP_USE_DRM
    auto* display = lv_linux_drm_create();
    if (!display) {
        return nullptr;
    }

    if (lv_linux_drm_set_file(display, APP_DRM_DEVICE, APP_DRM_CONNECTOR_ID) != LV_RESULT_OK) {
        lv_display_delete(display);
        return nullptr;
    }

    platform::init_key_input(display);
    return display;
#else
    auto* display = lv_linux_fbdev_create();
    if (!display) {
        return nullptr;
    }

    if (lv_linux_fbdev_set_file(display, APP_FRAMEBUFFER_DEVICE) != LV_RESULT_OK) {
        lv_display_delete(display);
        return nullptr;
    }

    platform::init_key_input(display);
    return display;
#endif
}
#endif

} // namespace

int Application::run() {
    logger::Logger::init();
    logger::Logger::set_tag("zero-print");

    lv_init();

    AssetManager assets;
    for (const auto& root : assets.roots()) {
        LOG_INFO("asset root: {}", root.string());
    }

    viewmodel::BaseViewModel view_model;
    const std::string user_config_path = writable_config_path();
    std::string loaded_config_path = APP_CONFIG_FILE;
    if (user_config_path != APP_CONFIG_FILE) {
        std::error_code filesystem_error;
        if (std::filesystem::is_regular_file(user_config_path, filesystem_error)) {
            loaded_config_path = user_config_path;
        }
    }

    ApplicationConfig config;
    std::string config_error;
    if (load_application_config(loaded_config_path, config, config_error)) {
        view_model.set_dark_mode(config.dark_mode);
        view_model.set_printer_transport(config.printer_transport);
        view_model.set_dither_mode(config.dither_mode);
        view_model.set_ble_address(config.ble_address);
        view_model.set_usb_device(config.usb_device);
        LOG_INFO("loaded config: {}", loaded_config_path);
    } else {
        LOG_WARN("failed to load config {}: {}; using defaults", loaded_config_path, config_error);
    }

#if USE_DESKTOP
    DesktopSimulatorFrame simulator_frame(assets);
    auto* display = simulator_frame.display();
#else
    auto* display = init_device_display();
#endif
    if (!display) {
        LOG_ERROR("failed to initialize display");
        return 1;
    }

    auto* standard_font = assets.load_standard_font(14);
    view::apply_lvgl_theme(display,
                           view_model.is_dark_mode(),
                           standard_font ? standard_font : LV_FONT_DEFAULT);

#if USE_DESKTOP
    simulator_frame.bind_dark_mode(view_model.dark_mode_subject());
#endif

    services::CameraPreviewStream camera_preview;
    ScreenManager screen_manager(view_model, assets, camera_preview);
    screen_manager.start();
    ShortcutController shortcuts(view_model, user_config_path, camera_preview);

    ConfigPersistence config_persistence{user_config_path, view_model.is_dark_mode(), &view_model};
    auto* dark_mode_observer = lv_subject_add_observer(view_model.dark_mode_subject(),
                                                        persist_config_observer,
                                                        &config_persistence);

    bool running = true;
    auto* quit_observer = lv_subject_add_observer(view_model.quit_requested_subject(),
                                                  quit_requested_observer,
                                                  &running);

    LOG_INFO("LVGL app started at {}x{}", lv_display_get_horizontal_resolution(display),
             lv_display_get_vertical_resolution(display));
    while (running
#if USE_DESKTOP
           && simulator_frame.process_events()
#endif
    ) {
        lv_timer_handler();
        lv_delay_ms(5);
    }

    if (quit_observer) {
        lv_observer_remove(quit_observer);
    }
    if (dark_mode_observer) {
        lv_observer_remove(dark_mode_observer);
    }

    return 0;
}

} // namespace app
