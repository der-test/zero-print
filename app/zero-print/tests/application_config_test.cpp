#include "application_config.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

int main() {
    const auto path = std::filesystem::temp_directory_path() / "zero-print-config-test.conf";
    std::error_code filesystem_error;
    std::filesystem::remove(path, filesystem_error);

    app::ApplicationConfig expected;
    expected.dark_mode = true;
    expected.printer_transport = 1;
    expected.dither_mode = 2;
    expected.ble_address = "AA:BB:CC:DD:EE:FF";
    expected.usb_device = "/dev/usb/lp1";

    std::string error;
    assert(app::save_application_config(path.string(), expected, error));

    app::ApplicationConfig loaded;
    assert(app::load_application_config(path.string(), loaded, error));
    assert(loaded.dark_mode == expected.dark_mode);
    assert(loaded.printer_transport == expected.printer_transport);
    assert(loaded.dither_mode == expected.dither_mode);
    assert(loaded.ble_address == expected.ble_address);
    assert(loaded.usb_device == expected.usb_device);

    {
        std::ofstream invalid(path, std::ios::trunc);
        invalid << "[printer]\ndither_mode=9\n";
    }
    assert(!app::load_application_config(path.string(), loaded, error));
    assert(!error.empty());

    std::filesystem::remove(path, filesystem_error);
    return 0;
}