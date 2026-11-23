#include "printer_protocol.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

int main() {
    std::string error;
    std::vector<uint8_t> job;
    imaging::RasterImage mx06{384, 2, 48, std::vector<uint8_t>(96, 0)};
    mx06.pixels[0] = 0x80;
    mx06.pixels[48] = 0x01;

    assert(printing::make_mx06_job(mx06, job, error));
    assert(error.empty());
    assert(job[0] == 0x51 && job[1] == 0x78 && job[2] == 0xaf);

    size_t row_command = 0;
    for (; row_command + 2 < job.size(); row_command++) {
        if (job[row_command] == 0x51 && job[row_command + 1] == 0x78 &&
            job[row_command + 2] == 0xa2) {
            break;
        }
    }
    assert(row_command + 56 <= job.size());
    assert(job[row_command + 6] == 0x00);
    assert(job[row_command + 6 + 47] == 0x01);
    assert(job[row_command + 56 + 6] == 0x00);
    assert(job[row_command + 56 + 6 + 47] == 0x80);

    imaging::RasterImage escpos{8, 1, 1, {0x80}};
    assert(printing::make_escpos_job(escpos, job, error));
    assert(job.size() == 18);
    assert(job[0] == 0x1b && job[1] == 0x40);
    assert(job[2] == 0x1d && job[3] == 0x76 && job[4] == 0x30);
    assert(job[6] == 1 && job[7] == 0);
    assert(job[8] == 1 && job[9] == 0);
    assert(job[10] == 0x80);
    assert(job[11] == 0x1b && job[12] == 0x64);
    assert(job[14] == 0x1d && job[15] == 0x56);

    imaging::RasterImage invalid{8, 1, 1, {0, 0}};
    assert(!printing::make_escpos_job(invalid, job, error));
    assert(!error.empty());
    return 0;
}