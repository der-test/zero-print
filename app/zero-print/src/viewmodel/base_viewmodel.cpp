/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "base_viewmodel.h"

#include <algorithm>

namespace viewmodel {
namespace {

int page_to_int(model::AppPage page) {
    return static_cast<int>(page);
}

} // namespace

BaseViewModel::BaseViewModel()
    : title_subject_(model_.app_title()),
      greeting_subject_(model_.greeting()),
      dark_mode_subject_(model_.dark_mode()),
      current_page_subject_(page_to_int(model_.current_page())),
      counter_subject_(0),
      bold_text_subject_(false),
      info_visible_subject_(false),
    quit_requested_subject_(false),
    home_selection_subject_(0),
    settings_row_subject_(0),
    printer_transport_subject_(0),
    dither_mode_subject_(0),
    device_label_subject_("Scan for a printer"),
    status_subject_("Ready"),
    photo_path_subject_("") {}

BaseViewModel::~BaseViewModel() = default;

lv_subject_t* BaseViewModel::title_subject() {
    return title_subject_.native();
}

lv_subject_t* BaseViewModel::greeting_subject() {
    return greeting_subject_.native();
}

lv_subject_t* BaseViewModel::dark_mode_subject() {
    return dark_mode_subject_.native();
}

lv_subject_t* BaseViewModel::current_page_subject() {
    return current_page_subject_.native();
}

lv_subject_t* BaseViewModel::counter_subject() {
    return counter_subject_.native();
}

lv_subject_t* BaseViewModel::bold_text_subject() {
    return bold_text_subject_.native();
}

lv_subject_t* BaseViewModel::info_visible_subject() {
    return info_visible_subject_.native();
}

lv_subject_t* BaseViewModel::quit_requested_subject() {
    return quit_requested_subject_.native();
}

lv_subject_t* BaseViewModel::home_selection_subject() {
    return home_selection_subject_.native();
}

lv_subject_t* BaseViewModel::settings_row_subject() {
    return settings_row_subject_.native();
}

lv_subject_t* BaseViewModel::printer_transport_subject() {
    return printer_transport_subject_.native();
}

lv_subject_t* BaseViewModel::dither_mode_subject() {
    return dither_mode_subject_.native();
}

lv_subject_t* BaseViewModel::device_label_subject() {
    return device_label_subject_.native();
}

lv_subject_t* BaseViewModel::status_subject() {
    return status_subject_.native();
}

lv_subject_t* BaseViewModel::photo_path_subject() {
    return photo_path_subject_.native();
}

bool BaseViewModel::is_dark_mode() const {
    return model_.dark_mode();
}

void BaseViewModel::set_dark_mode(bool enabled) {
    model_.set_dark_mode(enabled);
    publish_all();
}

void BaseViewModel::toggle_dark_mode() {
    model_.toggle_dark_mode();
    publish_all();
}

model::AppPage BaseViewModel::current_page() const {
    return model_.current_page();
}

void BaseViewModel::show_apple_page() {
    model_.set_current_page(model::AppPage::Apple);
    publish_all();
}

void BaseViewModel::show_butter_page() {
    model_.set_current_page(model::AppPage::Butter);
    publish_all();
}

void BaseViewModel::toggle_page() {
    model_.toggle_page();
    publish_all();
}

void BaseViewModel::increment_counter() {
    counter_subject_.set(counter_subject_.value() + 1);
}

void BaseViewModel::decrement_counter() {
    const auto next = counter_subject_.value() - 1;
    counter_subject_.set(next < 0 ? 0 : next);
}

void BaseViewModel::toggle_bold_text() {
    bold_text_subject_.toggle();
}

void BaseViewModel::toggle_info() {
    info_visible_subject_.toggle();
}

void BaseViewModel::request_quit() {
    quit_requested_subject_.set(true);
}

void BaseViewModel::show_home_page() {
    model_.set_current_page(model::AppPage::Home);
    publish_all();
}

void BaseViewModel::show_settings_page() {
    model_.set_current_page(model::AppPage::Settings);
    publish_all();
}

void BaseViewModel::show_camera_page() {
    model_.set_current_page(model::AppPage::Camera);
    publish_all();
}

void BaseViewModel::show_preview_page() {
    model_.set_current_page(model::AppPage::Preview);
    publish_all();
}

void BaseViewModel::back() {
    switch (model_.current_page()) {
        case model::AppPage::Settings:
        case model::AppPage::Camera:
            show_home_page();
            break;
        case model::AppPage::Preview:
            set_status("Ready to capture");
            show_camera_page();
            break;
        case model::AppPage::Home:
        default:
            break;
    }
}

int BaseViewModel::home_selection() const {
    return home_selection_;
}

int BaseViewModel::settings_row() const {
    return settings_row_;
}

int BaseViewModel::printer_transport() const {
    return printer_transport_;
}

int BaseViewModel::dither_mode() const {
    return dither_mode_;
}

const std::string& BaseViewModel::ble_address() const {
    return ble_address_;
}

const std::string& BaseViewModel::usb_device() const {
    return usb_device_;
}

const std::string& BaseViewModel::photo_path() const {
    return photo_path_;
}

void BaseViewModel::move_home_selection(int delta) {
    home_selection_ = (home_selection_ + delta + 2) % 2;
    home_selection_subject_.set(home_selection_);
}

void BaseViewModel::move_settings_row(int delta) {
    settings_row_ = (settings_row_ + delta + 3) % 3;
    settings_row_subject_.set(settings_row_);
}

void BaseViewModel::cycle_printer_transport(int delta) {
    printer_transport_ = (printer_transport_ + delta + 2) % 2;
    printer_transport_subject_.set(printer_transport_);
    device_label_subject_.set(printer_transport_ == 0
                                  ? (ble_address_.empty() ? "Scan for MX06" : ble_address_.c_str())
                                  : (usb_device_.empty() ? "Scan for USB printer" : usb_device_.c_str()));
}

void BaseViewModel::cycle_dither_mode(int delta) {
    dither_mode_ = (dither_mode_ + delta + 3) % 3;
    dither_mode_subject_.set(dither_mode_);
}

void BaseViewModel::set_printer_transport(int transport) {
    printer_transport_ = std::clamp(transport, 0, 1);
    printer_transport_subject_.set(printer_transport_);
    device_label_subject_.set(printer_transport_ == 0
                                  ? (ble_address_.empty() ? "Scan for MX06" : ble_address_.c_str())
                                  : (usb_device_.empty() ? "Scan for USB printer" : usb_device_.c_str()));
}

void BaseViewModel::set_dither_mode(int mode) {
    dither_mode_ = std::clamp(mode, 0, 2);
    dither_mode_subject_.set(dither_mode_);
}

void BaseViewModel::set_ble_address(const std::string& address) {
    ble_address_ = address;
    if (printer_transport_ == 0) {
        device_label_subject_.set(ble_address_.empty() ? "Scan for MX06" : ble_address_.c_str());
    }
}

void BaseViewModel::set_usb_device(const std::string& device) {
    usb_device_ = device;
    if (printer_transport_ == 1) {
        device_label_subject_.set(usb_device_.empty() ? "Scan for USB printer" : usb_device_.c_str());
    }
}

void BaseViewModel::set_status(const std::string& status) {
    status_subject_.set(status.c_str());
}

void BaseViewModel::set_photo_path(const std::string& path) {
    photo_path_ = path;
    photo_path_subject_.set(photo_path_.c_str());
}

void BaseViewModel::publish_all() {
    title_subject_.set(model_.app_title());
    greeting_subject_.set(model_.greeting());
    dark_mode_subject_.set(model_.dark_mode());
    current_page_subject_.set(page_to_int(model_.current_page()));
}

} // namespace viewmodel
