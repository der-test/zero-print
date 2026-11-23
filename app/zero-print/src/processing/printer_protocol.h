#pragma once

#include "raster.h"

#include <cstdint>
#include <string>
#include <vector>

namespace printing {

bool make_mx06_job(const imaging::RasterImage& raster,
                   std::vector<uint8_t>& output,
                   std::string& error);

bool make_escpos_job(const imaging::RasterImage& raster,
                     std::vector<uint8_t>& output,
                     std::string& error);

} // namespace printing