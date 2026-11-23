#include "workflow_screens.h"

#include "asset_manager.h"
#include "bindings.h"
#include "camera_service.h"
#include "theme.h"
#include "ui_const.h"

#include <algorithm>
#include <array>
#include <string>

namespace screen {
namespace {

constexpr std::array<const char*, 3> kDitherNames = {
    "Floyd-Steinberg",
    "Atkinson",
    "Threshold",
};

void set_label_font(lv_obj_t* label,
                    app::AssetManager& assets,
                    uint16_t size,
                    app::StandardFontWeight weight = app::StandardFontWeight::Regular) {
    const lv_font_t* font = assets.load_standard_font(size, weight);
    lv_obj_set_style_text_font(label,
                               font ? font : &lv_font_montserrat_16,
                               0);
}

lv_obj_t* create_row(lv_obj_t* parent, const char* title, app::AssetManager& assets) {
    auto* row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, 288, 34);
    lv_obj_set_style_radius(row, 4, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_color(row, lv_color_hex(0xd8dce0), 0);
    lv_obj_set_style_bg_color(row, lv_color_hex(0xf3f4f4), 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_hor(row, 9, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    auto* title_label = lv_label_create(row);
    lv_label_set_text(title_label, title);
    set_label_font(title_label, assets, 14);
    return row;
}

lv_obj_t* create_value_label(lv_obj_t* row, app::AssetManager& assets) {
    auto* value = lv_label_create(row);
    lv_label_set_long_mode(value, LV_LABEL_LONG_DOT);
    lv_obj_set_width(value, 160);
    lv_obj_set_style_text_align(value, LV_TEXT_ALIGN_RIGHT, 0);
    set_label_font(value, assets, 13, app::StandardFontWeight::Bold);
    return value;
}

} // namespace

HomeScreen::HomeScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets)
    : BaseScreen(view_model, assets) {
    init();
}

void HomeScreen::build_content(lv_obj_t* content) {
    auto* options = lv_obj_create(content);
    lv_obj_remove_style_all(options);
    lv_obj_set_size(options, 304, 102);
    lv_obj_set_flex_flow(options, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(options, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(options, 6, 0);
    lv_obj_align(options, LV_ALIGN_TOP_MID, 0, 7);
    lv_obj_clear_flag(options, LV_OBJ_FLAG_SCROLLABLE);

    const std::array<const char*, 2> titles = {"CAMERA", "SETTINGS"};
    const std::array<const char*, 2> subtitles = {"SNAP + PRINT", "BLE / USB"};
    for (size_t index = 0; index < buttons_.size(); index++) {
        auto* button = lv_button_create(options);
        buttons_[index] = button;
        lv_obj_set_size(button, 144, 84);
        lv_obj_set_style_radius(button, 5, 0);
        lv_obj_set_style_bg_color(button,
                                  index == 0 ? lv_color_hex(0x145d4f) : lv_color_hex(0x343b42),
                                  0);
        lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(button, 2, 0);
        lv_obj_set_style_border_color(button, lv_color_hex(0x343b42), 0);
        lv_obj_set_flex_flow(button, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(button, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_row(button, 5, 0);

        auto* title = lv_label_create(button);
        lv_label_set_text(title, titles[index]);
        set_label_font(title, assets(), 18, app::StandardFontWeight::Bold);
        lv_obj_set_style_text_color(title, lv_color_white(), 0);

        auto* subtitle = lv_label_create(button);
        lv_label_set_text(subtitle, subtitles[index]);
        set_label_font(subtitle, assets(), 11);
        lv_obj_set_style_text_color(subtitle, lv_color_hex(0xe4e8e8), 0);
    }

    lv_obj_add_event_cb(buttons_[0], camera_clicked, LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(buttons_[1], settings_clicked, LV_EVENT_CLICKED, this);
    lv_subject_add_observer_obj(view_model().home_selection_subject(), selection_changed, options, this);
    refresh_selection();

    auto* key_hints = lv_label_create(content);
    lv_obj_set_width(key_hints, 304);
    lv_label_set_long_mode(key_hints, LV_LABEL_LONG_DOT);
    lv_label_set_text(key_hints, "Up/Down Select | Enter/OK Open | Hold Esc Quit");
    set_label_font(key_hints, assets(), 10);
    lv_obj_align(key_hints, LV_ALIGN_BOTTOM_MID, 0, -2);
    reactive::bind_theme(key_hints, view_model().dark_mode_subject(), reactive::ThemeRole::Text);
}

void HomeScreen::refresh_selection() {
    for (size_t index = 0; index < buttons_.size(); index++) {
        lv_obj_set_style_border_color(buttons_[index],
                                      static_cast<int>(index) == view_model().home_selection()
                                          ? lv_color_hex(0xf0a629)
                                          : lv_color_hex(0x343b42),
                                      0);
    }
}

void HomeScreen::selection_changed(lv_observer_t* observer, lv_subject_t*) {
    auto* screen = static_cast<HomeScreen*>(lv_observer_get_user_data(observer));
    if (screen) {
        screen->refresh_selection();
    }
}

void HomeScreen::camera_clicked(lv_event_t* event) {
    auto* screen = static_cast<HomeScreen*>(lv_event_get_user_data(event));
    if (screen) {
        screen->view_model().show_camera_page();
    }
}

void HomeScreen::settings_clicked(lv_event_t* event) {
    auto* screen = static_cast<HomeScreen*>(lv_event_get_user_data(event));
    if (screen) {
        screen->view_model().show_settings_page();
    }
}

SettingsScreen::SettingsScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets)
    : BaseScreen(view_model, assets) {
    init();
}

void SettingsScreen::build_content(lv_obj_t* content) {
    auto* rows = lv_obj_create(content);
    lv_obj_remove_style_all(rows);
    lv_obj_set_size(rows, 304, 112);
    lv_obj_set_flex_flow(rows, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(rows, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(rows, 3, 0);
    lv_obj_align(rows, LV_ALIGN_TOP_MID, 0, 3);
    lv_obj_clear_flag(rows, LV_OBJ_FLAG_SCROLLABLE);

    rows_[0] = create_row(rows, "CONNECTION", assets());
    transport_value_ = create_value_label(rows_[0], assets());
    rows_[1] = create_row(rows, "PRINTER", assets());
    device_value_ = create_value_label(rows_[1], assets());
    lv_label_bind_text(device_value_, view_model().device_label_subject(), nullptr);
    rows_[2] = create_row(rows, "DITHER", assets());
    dither_value_ = create_value_label(rows_[2], assets());

    settings_hint_ = lv_label_create(content);
    lv_obj_set_size(settings_hint_, 304, 24);
    lv_label_set_long_mode(settings_hint_, LV_LABEL_LONG_WRAP);
    set_label_font(settings_hint_, assets(), 9);
    lv_obj_align(settings_hint_, LV_ALIGN_BOTTOM_MID, 0, -1);
    reactive::bind_theme(settings_hint_, view_model().dark_mode_subject(), reactive::ThemeRole::Text);

    lv_subject_add_observer_obj(view_model().settings_row_subject(), selection_changed, rows, this);
    lv_subject_add_observer_obj(view_model().printer_transport_subject(), transport_changed, transport_value_, nullptr);
    lv_subject_add_observer_obj(view_model().dither_mode_subject(), dither_changed, dither_value_, nullptr);
    refresh_selection();
    lv_label_set_text(transport_value_, view_model().printer_transport() == 0
                                            ? "MX06 - BLE"
                                            : "USB - ESC/POS");
    lv_label_set_text(dither_value_,
                      kDitherNames[std::clamp(view_model().dither_mode(), 0, 2)]);
}

void SettingsScreen::refresh_selection() {
    for (size_t index = 0; index < rows_.size(); index++) {
        const bool selected = static_cast<int>(index) == view_model().settings_row();
        lv_obj_set_style_border_width(rows_[index], selected ? 2 : 1, 0);
        lv_obj_set_style_border_color(rows_[index],
                                      selected ? lv_color_hex(0xe49b25) : lv_color_hex(0xd8dce0),
                                      0);
    }
    switch (view_model().settings_row()) {
        case 0:
            lv_label_set_text(settings_hint_, "Up/Down: row  Left/Right: mode  Enter: scan  Esc: back");
            break;
        case 1:
            lv_label_set_text(settings_hint_, "Up/Down: row  Left/Right: rescan  Enter: scan  Esc: back");
            break;
        default:
            lv_label_set_text(settings_hint_, "Up/Down: row  Left/Right/Enter: dither  Esc: back");
            break;
    }
}

void SettingsScreen::selection_changed(lv_observer_t* observer, lv_subject_t*) {
    auto* screen = static_cast<SettingsScreen*>(lv_observer_get_user_data(observer));
    if (screen) {
        screen->refresh_selection();
    }
}

void SettingsScreen::transport_changed(lv_observer_t* observer, lv_subject_t* subject) {
    if (!observer || !subject) {
        return;
    }
    auto* label = lv_observer_get_target_obj(observer);
    if (!label) {
        return;
    }
    lv_label_set_text(label, lv_subject_get_int(subject) == 0 ? "MX06 - BLE" : "USB - ESC/POS");
}

void SettingsScreen::dither_changed(lv_observer_t* observer, lv_subject_t* subject) {
    if (!observer || !subject) {
        return;
    }
    auto* label = lv_observer_get_target_obj(observer);
    if (!label) {
        return;
    }
    const auto index = std::clamp<int32_t>(lv_subject_get_int(subject), 0, kDitherNames.size() - 1);
    lv_label_set_text(label, kDitherNames[index]);
}

CameraScreen::CameraScreen(viewmodel::BaseViewModel& view_model,
                           app::AssetManager& assets,
                           services::CameraPreviewStream& camera_stream)
    : BaseScreen(view_model, assets), camera_stream_(camera_stream) {
    init();
}

CameraScreen::~CameraScreen() {
    if (preview_timer_) {
        lv_timer_delete(preview_timer_);
        preview_timer_ = nullptr;
    }
    camera_stream_.stop();
}

void CameraScreen::build_content(lv_obj_t* content) {
    auto* frame = lv_obj_create(content);
    lv_obj_remove_style_all(frame);
    lv_obj_set_size(frame, 192, 138);
    lv_obj_set_style_radius(frame, 4, 0);
    lv_obj_set_style_bg_color(frame, lv_color_hex(0x181d20), 0);
    lv_obj_set_style_bg_opa(frame, LV_OPA_COVER, 0);
    lv_obj_align(frame, LV_ALIGN_LEFT_MID, 2, 0);
    lv_obj_clear_flag(frame, LV_OBJ_FLAG_SCROLLABLE);

    camera_image_obj_ = lv_image_create(frame);
    lv_obj_set_size(camera_image_obj_, 176, 132);
    lv_obj_center(camera_image_obj_);

    auto* details = lv_obj_create(content);
    lv_obj_remove_style_all(details);
    lv_obj_set_size(details, 116, 138);
    lv_obj_set_flex_flow(details, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(details, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(details, 7, 0);
    lv_obj_align(details, LV_ALIGN_RIGHT_MID, -4, 0);
    lv_obj_clear_flag(details, LV_OBJ_FLAG_SCROLLABLE);

    auto* title = lv_label_create(details);
    lv_label_set_text(title, "LIVE VIEW");
    set_label_font(title, assets(), 15, app::StandardFontWeight::Bold);

    auto* sensor = lv_label_create(details);
    lv_label_set_text(sensor, "IMX219");
    set_label_font(sensor, assets(), 12);

    auto* printer = lv_label_create(details);
    lv_label_set_text(printer,
                      view_model().printer_transport() == 0 ? "MX06 - BLE" : "USB - ESC/POS");
    set_label_font(printer, assets(), 11);

    auto* capture_hint = lv_label_create(details);
    lv_label_set_text(capture_hint, "Enter/OK Capture");
    set_label_font(capture_hint, assets(), 10);

    auto* camera_back_hint = lv_label_create(details);
    lv_label_set_text(camera_back_hint, "Esc Home | Hold Esc Quit");
    lv_obj_set_width(camera_back_hint, 112);
    lv_label_set_long_mode(camera_back_hint, LV_LABEL_LONG_WRAP);
    set_label_font(camera_back_hint, assets(), 9);

    auto* status = lv_label_create(details);
    lv_obj_set_width(status, 112);
    lv_label_set_long_mode(status, LV_LABEL_LONG_WRAP);
    lv_label_bind_text(status, view_model().status_subject(), nullptr);
    set_label_font(status, assets(), 11);

    std::string error;
    if (camera_stream_.start(error)) {
        view_model().set_status(view_model().printer_transport() == 0
                                    ? "Live view - MX06 BLE"
                                    : "Live view - USB ESC/POS");
    } else {
        view_model().set_status(error);
    }
    preview_timer_ = lv_timer_create(preview_timer_cb, 80, this);
}

void CameraScreen::refresh_preview() {
    std::vector<uint8_t> jpeg_frame;
    std::string error;
    if (!camera_stream_.read_frame(jpeg_frame, error)) {
        if (!error.empty()) {
            view_model().set_status(error);
        }
        return;
    }

    imaging::GrayImage source;
    imaging::GrayImage oriented;
    imaging::GrayImage preview;
    if (!services::decode_jpeg_grayscale(jpeg_frame.data(), jpeg_frame.size(), source, error) ||
        !imaging::rotate_180(source, oriented, error) ||
        !imaging::resize_to_width(oriented, 176, preview, error)) {
        view_model().set_status(error);
        return;
    }

    camera_pixels_.resize(preview.width * preview.height * 3);
    for (size_t index = 0; index < preview.width * preview.height; index++) {
        camera_pixels_[index * 3] = preview.pixels[index];
        camera_pixels_[index * 3 + 1] = preview.pixels[index];
        camera_pixels_[index * 3 + 2] = preview.pixels[index];
    }

    camera_image_.header.magic = LV_IMAGE_HEADER_MAGIC;
    camera_image_.header.cf = LV_COLOR_FORMAT_RGB888;
    camera_image_.header.w = static_cast<uint16_t>(preview.width);
    camera_image_.header.h = static_cast<uint16_t>(preview.height);
    camera_image_.data_size = static_cast<uint32_t>(camera_pixels_.size());
    camera_image_.data = camera_pixels_.data();
    lv_image_set_src(camera_image_obj_, &camera_image_);
}

void CameraScreen::preview_timer_cb(lv_timer_t* timer) {
    auto* screen = static_cast<CameraScreen*>(lv_timer_get_user_data(timer));
    if (screen) {
        screen->refresh_preview();
    }
}

PreviewScreen::PreviewScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets)
    : BaseScreen(view_model, assets) {
    init();
}

void PreviewScreen::build_content(lv_obj_t* content) {
    auto* photo_frame = lv_obj_create(content);
    lv_obj_remove_style_all(photo_frame);
    lv_obj_set_size(photo_frame, 174, 130);
    lv_obj_set_style_radius(photo_frame, 4, 0);
    lv_obj_set_style_bg_color(photo_frame, lv_color_hex(0x202428), 0);
    lv_obj_set_style_bg_opa(photo_frame, LV_OPA_COVER, 0);
    lv_obj_align(photo_frame, LV_ALIGN_LEFT_MID, 7, 0);
    lv_obj_clear_flag(photo_frame, LV_OBJ_FLAG_SCROLLABLE);

    imaging::GrayImage source;
    imaging::GrayImage oriented;
    imaging::GrayImage preview;
    std::string error;
    if (services::load_jpeg_grayscale(view_model().photo_path(), source, error) &&
        imaging::rotate_180(source, oriented, error) &&
        imaging::resize_to_width(oriented, 160, preview, error)) {
        preview_pixels_.resize(preview.width * preview.height * 3);
        for (size_t index = 0; index < preview.width * preview.height; index++) {
            preview_pixels_[index * 3] = preview.pixels[index];
            preview_pixels_[index * 3 + 1] = preview.pixels[index];
            preview_pixels_[index * 3 + 2] = preview.pixels[index];
        }

        preview_image_.header.magic = LV_IMAGE_HEADER_MAGIC;
        preview_image_.header.cf = LV_COLOR_FORMAT_RGB888;
        preview_image_.header.w = static_cast<uint16_t>(preview.width);
        preview_image_.header.h = static_cast<uint16_t>(preview.height);
        preview_image_.data_size = static_cast<uint32_t>(preview_pixels_.size());
        preview_image_.data = preview_pixels_.data();
        auto* image = lv_image_create(photo_frame);
        lv_image_set_src(image, &preview_image_);
        lv_obj_center(image);
    } else {
        auto* unavailable = lv_label_create(photo_frame);
        lv_label_set_text(unavailable, "PHOTO UNAVAILABLE");
        set_label_font(unavailable, assets(), 12);
        lv_obj_center(unavailable);
    }

    auto* details = lv_obj_create(content);
    lv_obj_remove_style_all(details);
    lv_obj_set_size(details, 126, 128);
    lv_obj_set_flex_flow(details, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(details, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(details, 7, 0);
    lv_obj_align(details, LV_ALIGN_RIGHT_MID, -7, 0);
    lv_obj_clear_flag(details, LV_OBJ_FLAG_SCROLLABLE);

    auto* state = lv_label_create(details);
    lv_label_set_text(state, "READY TO PRINT");
    set_label_font(state, assets(), 14, app::StandardFontWeight::Bold);

    auto* dither = lv_label_create(details);
    lv_label_set_text(dither, kDitherNames[std::clamp(view_model().dither_mode(), 0, 2)]);
    set_label_font(dither, assets(), 11);

    auto* printer = lv_label_create(details);
    lv_label_set_text(printer,
                      view_model().printer_transport() == 0 ? "MX06 - BLE" : "USB - ESC/POS");
    set_label_font(printer, assets(), 11);

    auto* print_hint = lv_label_create(details);
    lv_label_set_text(print_hint, "Enter/OK Print");
    set_label_font(print_hint, assets(), 10);

    auto* retake_hint = lv_label_create(details);
    lv_label_set_text(retake_hint, "Esc Retake | Hold Esc Quit");
    lv_obj_set_width(retake_hint, 118);
    lv_label_set_long_mode(retake_hint, LV_LABEL_LONG_WRAP);
    set_label_font(retake_hint, assets(), 9);

    auto* status = lv_label_create(details);
    lv_label_set_long_mode(status, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(status, 118);
    lv_label_bind_text(status, view_model().status_subject(), nullptr);
    set_label_font(status, assets(), 11);
}

} // namespace screen