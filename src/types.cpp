#include "relayforge/types.hpp"
#include <random>
#include <sstream>
#include <iomanip>

namespace relayforge {

std::string generate_uuid_v4() {
    static thread_local std::random_device rd;
    static thread_local std::mt19937_64 gen(rd());
    static thread_local std::uniform_int_distribution<uint64_t> dis;

    uint64_t part1 = dis(gen);
    uint64_t part2 = dis(gen);

    // Set version to 4 (0100 in bits 12-15 of time_hi_and_version)
    part1 = (part1 & 0xFFFFFFFFFFFF0FFFULL) | 0x0000000000004000ULL;
    // Set variant to 10xx in bits 6-7 of clock_seq_hi_and_reserved
    part2 = (part2 & 0x3FFFFFFFFFFFFFFFULL) | 0x8000000000000000ULL;

    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    ss << std::setw(8) << (part1 >> 32);
    ss << "-";
    ss << std::setw(4) << ((part1 >> 16) & 0xFFFF);
    ss << "-";
    ss << std::setw(4) << (part1 & 0xFFFF);
    ss << "-";
    ss << std::setw(4) << (part2 >> 48);
    ss << "-";
    ss << std::setw(12) << (part2 & 0xFFFFFFFFFFFFULL);

    return ss.str();
}

} // namespace relayforge
