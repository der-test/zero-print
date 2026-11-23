/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "subjects.h"
#include "base_model.h"

#include "lvgl.h"

#include <string>

namespace viewmodel {

class BaseViewModel {
public:
    BaseViewModel();
    ~BaseViewModel();

    BaseViewModel(const BaseViewModel&) = delete;
    BaseViewModel& operator=(const BaseViewModel&) = delete;

    lv_subject_t* title_subject();
    lv_subject_t* greeting_subject();
    lv_subject_t* dark_mode_subject();
    lv_subject_t* current_page_subject();
    lv_subject_t* counter_subject();
    lv_subject_t* bold_text_subject();
    lv_subject_t* info_visible_subject();
    lv_subject_t* quit_requested_subject();
    lv_subject_t* home_selection_subject();
    lv_subject_t* settings_row_subject();
    lv_subject_t* printer_transport_subject();
    lv_subject_t* dither_mode_subject();
    lv_subject_t* device_label_subject();
    lv_subject_t* status_subject();
    lv_subject_t* photo_path_subject();

    bool is_dark_mode() const;
    void set_dark_mode(bool enabled);
    void toggle_dark_mode();

    model::AppPage current_page() const;
    void show_apple_page();
    void show_butter_page();
    void toggle_page();
    void increment_counter();
    void decrement_counter();
    void toggle_bold_text();
    void toggle_info();
    void request_quit();
    void show_home_page();
    void show_settings_page();
    void show_camera_page();
    void show_preview_page();
    void back();
    int home_selection() const;
    int settings_row() const;
    int printer_transport() const;
    int dither_mode() const;
    const std::string& ble_address() const;
    const std::string& usb_device() const;
    const std::string& photo_path() const;
    void move_home_selection(int delta);
    void move_settings_row(int delta);
    void cycle_printer_transport(int delta);
    void cycle_dither_mode(int delta);
    void set_printer_transport(int transport);
    void set_dither_mode(int mode);
    void set_ble_address(const std::string& address);
    void set_usb_device(const std::string& device);
    void set_status(const std::string& status);
    void set_photo_path(const std::string& path);

private:
    void publish_all();

    model::BaseModel model_;
    reactive::StringSubject<32> title_subject_;
    reactive::StringSubject<32> greeting_subject_;
    reactive::BoolSubject dark_mode_subject_;
    reactive::IntSubject current_page_subject_;
    reactive::IntSubject counter_subject_;
    reactive::BoolSubject bold_text_subject_;
    reactive::BoolSubject info_visible_subject_;
    reactive::BoolSubject quit_requested_subject_;
    reactive::IntSubject home_selection_subject_;
    reactive::IntSubject settings_row_subject_;
    reactive::IntSubject printer_transport_subject_;
    reactive::IntSubject dither_mode_subject_;
    reactive::StringSubject<96> device_label_subject_;
    reactive::StringSubject<128> status_subject_;
    reactive::StringSubject<260> photo_path_subject_;
    int home_selection_ = 0;
    int settings_row_ = 0;
    int printer_transport_ = 0;
    int dither_mode_ = 0;
    std::string ble_address_;
    std::string usb_device_;
    std::string photo_path_;
};

} // namespace viewmodel
