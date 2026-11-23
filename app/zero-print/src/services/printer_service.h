#pragma once

#include "printer_protocol.h"

#include <string>
#include <vector>

namespace services {

enum class PrinterTransport {
    BleMx06,
    UsbEscpos,
};

struct PrinterConfig {
    PrinterTransport transport = PrinterTransport::BleMx06;
    std::string ble_address;
    std::string usb_device = "/dev/usb/lp0";
};

bool scan_mx06(std::string& address, std::string& error);
std::vector<std::string> scan_usb_printers();
bool print_raster(const imaging::RasterImage& raster,
                  PrinterConfig& config,
                  std::string& error);

} // namespace services