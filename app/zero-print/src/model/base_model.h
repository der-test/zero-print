/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

namespace model {

enum class AppPage {
    Home = 0,
    Settings = 1,
    Camera = 2,
    Preview = 3,
    Apple = Home,
    Butter = Settings,
};

class BaseModel {
public:
    const char* app_title() const;
    const char* greeting() const;

    bool dark_mode() const;
    void set_dark_mode(bool enabled);
    void toggle_dark_mode();

    AppPage current_page() const;
    void set_current_page(AppPage page);
    void toggle_page();

private:
    bool dark_mode_ = false;
    AppPage current_page_ = AppPage::Apple;
};

} // namespace model
