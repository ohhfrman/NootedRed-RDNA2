// Descriptive bring-up state only: observed kext loading is NOT hardware acceleration.
#pragma once
#include <stdint.h>
namespace MendocinoBringup {
enum Bit : uint32_t {
    PciIdentity = 1U << 0,
    PciDisplayClass = 1U << 1,
    AtomVbiosPresent = 1U << 2,
    X6000FramebufferSeen = 1U << 3,
    X6000AcceleratorSeen = 1U << 4,
    X6000HWLibsSeen = 1U << 5,
    // These bits MUST be set only by future runtime verification, not kext arrival.
    DCN316Initialized = 1U << 6,
    GC1037Initialized = 1U << 7,
    MetalVerified = 1U << 8,
    IPDiscoveryValidated = 1U << 9,
    GC1037Identified = 1U << 10,
    DCN316Identified = 1U << 11,
    EDIDTimingValidated = 1U << 12,
    VbiosHandoffComplete = 1U << 13,
};
struct State {
    uint32_t observations{0};
    constexpr void observe(Bit b) { observations |= static_cast<uint32_t>(b); }
    constexpr bool has(Bit b) const { return (observations & static_cast<uint32_t>(b)) != 0; }
    constexpr bool diagnosticsComplete() const {
        return has(PciIdentity) && has(PciDisplayClass) && has(AtomVbiosPresent) &&
               has(X6000FramebufferSeen) && has(X6000AcceleratorSeen) && has(X6000HWLibsSeen);
    }
    // Identification and preflight are NOT GPU/display engine initialization.
    constexpr bool hardwareProfileConfirmed() const {
        return has(PciIdentity) && has(PciDisplayClass) && has(IPDiscoveryValidated) &&
               has(GC1037Identified) && has(DCN316Identified);
    }
    constexpr bool accelerationVerified() const { return has(DCN316Initialized) && has(GC1037Initialized) && has(MetalVerified); }
};
}
