// AMD APU hardware profiles used to keep Vega and RDNA2 paths separate.
// Independent, dependency-free declarations for use in the kext and host tests.
#pragma once
#include <stdint.h>

namespace NRedHardware {
enum class Architecture : uint8_t { Unknown, Vega, MendocinoRDNA2 };

struct IPVersion {
    uint8_t major;
    uint8_t minor;
    uint8_t revision;

    constexpr uint32_t packed() const
    {
        return (static_cast<uint32_t>(major) << 16) | (static_cast<uint32_t>(minor) << 8) | revision;
    }
};

struct Profile {
    uint16_t vendorID;
    uint16_t deviceID;
    Architecture architecture;
    IPVersion gc;
    IPVersion dcn;
    IPVersion sdma;
    IPVersion vcn;
    IPVersion mp0;
    IPVersion mp1;
};

// AMD Ryzen 7020 (Mendocino), integrated Radeon 610M.
// Sources: Linux kernel AMD APU IP version documentation and pci.ids.
inline constexpr Profile mendocino = {
    0x1002, 0x1506, Architecture::MendocinoRDNA2,
    {10, 3, 7}, {3, 1, 6}, {5, 2, 7}, {3, 1, 1}, {13, 0, 8}, {13, 0, 8},
};

constexpr Architecture classify(const uint16_t vendorID, const uint16_t deviceID)
{
    if (vendorID != 0x1002) { return Architecture::Unknown; }
    switch (deviceID) {
        case 0x15D8: // Picasso / Raven2
        case 0x15DD: // Raven
        case 0x164C: // Lucienne
        case 0x1636: // Renoir
        case 0x15E7: // Barcelo
        case 0x1638: // Cezanne
            return Architecture::Vega;
        case 0x1506: // Mendocino Radeon 610M
            return Architecture::MendocinoRDNA2;
        default: return Architecture::Unknown;
    }
}

static_assert(classify(0x1002, 0x1506) == Architecture::MendocinoRDNA2);
static_assert(classify(0x1002, 0x1636) == Architecture::Vega);
static_assert(classify(0x1002, 0x1681) == Architecture::Unknown); // Rembrandt != Mendocino.
static_assert(classify(0x10DE, 0x1506) == Architecture::Unknown);
static_assert(mendocino.gc.packed() == 0x0A0307);
}
