// Mendocino PCI configuration-space decoding, independent of IOKit.
// Read-only discovery; decoding registers is not GPU initialization.
#pragma once
#include "HardwareProfile.hpp"
#include <stdint.h>

namespace NRedHardware {
constexpr uint16_t pciVendor(const uint32_t vendorDevice) { return vendorDevice & 0xFFFFU; }
constexpr uint16_t pciDevice(const uint32_t vendorDevice) { return (vendorDevice >> 16) & 0xFFFFU; }
constexpr uint8_t pciRevision(const uint32_t revisionClass) { return revisionClass & 0xFFU; }
constexpr uint8_t pciBaseClass(const uint32_t revisionClass) { return (revisionClass >> 24) & 0xFFU; }
constexpr uint8_t pciSubclass(const uint32_t revisionClass) { return (revisionClass >> 16) & 0xFFU; }
constexpr uint16_t pciSubVendor(const uint32_t subsystem) { return subsystem & 0xFFFFU; }
constexpr uint16_t pciSubDevice(const uint32_t subsystem) { return (subsystem >> 16) & 0xFFFFU; }
constexpr bool isGraphicsClass(const uint32_t classRevision) { return pciBaseClass(classRevision) == 0x03; }

struct BarDescriptor {
    bool ioSpace;
    bool is64Bit;
    bool prefetchable;
    uint32_t addressLow;
};

// PCI BAR values are *raw configuration values*, not resource sizes or mapped addresses.
// A BAR may be unassigned; no size probing is performed (that would require writes).
constexpr BarDescriptor decodeBAR(const uint32_t bar)
{
    const bool isIO = (bar & 1U) != 0;
    return {isIO, !isIO && (((bar >> 1) & 3U) == 2), !isIO && ((bar & 8U) != 0),
            isIO ? (bar & ~3U) : (bar & ~15U)};
}

struct PciSnapshot {
    uint32_t vendorDevice;
    uint32_t revisionClass;
    uint32_t subsystem;
    uint16_t command;
    uint32_t bar0;
    uint32_t bar2;
    uint32_t bar5;
};

enum class ProbeStatus : uint8_t {
    NotMendocino,
    UnexpectedPciClass,
    MendocinoPciVerified
};

constexpr ProbeStatus evaluate(const PciSnapshot& snap)
{
    if (classify(pciVendor(snap.vendorDevice), pciDevice(snap.vendorDevice)) != Architecture::MendocinoRDNA2) {
        return ProbeStatus::NotMendocino;
    }
    if (!isGraphicsClass(snap.revisionClass)) { return ProbeStatus::UnexpectedPciClass; }
    return ProbeStatus::MendocinoPciVerified;
}

static_assert(pciVendor(0x15061002) == 0x1002);
static_assert(pciDevice(0x15061002) == 0x1506);
static_assert(pciBaseClass(0x03000001) == 0x03);
static_assert(decodeBAR(0xFEDC0004).is64Bit);
static_assert(!decodeBAR(0xFEDC0004).ioSpace);
}
