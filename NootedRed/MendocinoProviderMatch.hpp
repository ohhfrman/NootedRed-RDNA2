// Stage 3C: *our* opt-in IOKit provider match, not Apple X6000 binding.
// Pure validation shared by kernel implementation and host tests.
#pragma once
#include <MendocinoPciConfig.hpp>
#include <stdint.h>

namespace MendocinoProviderMatch {

constexpr uint32_t kVendorDevice = 0x15061002U;

constexpr bool accepts(uint32_t vendorDevice, uint32_t classRevision,
                       bool explicitlyEnabled) {
    return explicitlyEnabled && vendorDevice == kVendorDevice &&
           NRedHardware::isGraphicsClass(classRevision);
}

// This is strictly an independent, read-only IOKit service. It cannot stand
// in for an X6000 controller, initialize GC/DCN, or validate Metal.
constexpr bool appleX6000ControllerBound() { return false; }
constexpr bool graphicsInitialized() { return false; }

static_assert(accepts(0x15061002U, 0x030000C2U, true));
static_assert(!accepts(0x15061002U, 0x030000C2U, false));
static_assert(!accepts(0x15071002U, 0x030000C2U, true));
static_assert(!accepts(0x150610DEU, 0x030000C2U, true));
static_assert(!accepts(0x15061002U, 0x020000C2U, true));

} // namespace MendocinoProviderMatch
