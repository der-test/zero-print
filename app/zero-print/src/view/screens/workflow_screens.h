#pragma once

#include "base_screen.h"
#include "camera_service.h"

#include <array>
#include <cstdint>
#include <vector>

namespace screen {

class HomeScreen : public BaseScreen {
public:
    HomeScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets);

private:
    void build_content(lv_obj_t* content) override;
    void refresh_selection();
    static void selection_changed(lv_observer_t* observer, lv_subject_t* subject);
    static void camera_clicked(lv_event_t* event);
    static void settings_clicked(lv_event_t* event);

    std::array<lv_obj_t*, 2> buttons_{};
};

class SettingsScreen : public BaseScreen {
public:
    SettingsScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets);

private:
    void build_content(lv_obj_t* content) override;
    void refresh_selection();
    static void selection_changed(lv_observer_t* observer, lv_subject_t* subject);
    static void transport_changed(lv_observer_t* observer, lv_subject_t* subject);
    static void dither_changed(lv_observer_t* observer, lv_subject_t* subject);

    std::array<lv_obj_t*, 3> rows_{};
    lv_obj_t* settings_hint_{nullptr};
    lv_obj_t* transport_value_{nullptr};
    lv_obj_t* device_value_{nullptr};
    lv_obj_t* dither_value_{nullptr};
};

class CameraScreen : public BaseScreen {
public:
    CameraScreen(viewmodel::BaseViewModel& view_model,
                 app::AssetManager& assets,
                 services::CameraPreviewStream& camera_stream);
    ~CameraScreen() override;

private:
    void build_content(lv_obj_t* content) override;
    void refresh_preview();
    static void preview_timer_cb(lv_timer_t* timer);

    services::CameraPreviewStream& camera_stream_;
    lv_timer_t* preview_timer_{nullptr};
    lv_obj_t* camera_image_obj_{nullptr};
    lv_image_dsc_t camera_image_{};
    std::vector<uint8_t> camera_pixels_;
};

class PreviewScreen : public BaseScreen {
public:
    PreviewScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets);

private:
    void build_content(lv_obj_t* content) override;

    lv_image_dsc_t preview_image_{};
    std::vector<uint8_t> preview_pixels_;
};

} // namespace screen