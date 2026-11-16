// R6 native X6000 compatibility gate for AMD Mendocino (1002:1506).
// This is an eligibility decision, NOT a native driver attach or acceleration claim.
// Do not enable the X5000/Vega patches or bind arbitrary Navi personalities here.
#pragma once
#include <MendocinoBringup.hpp>
#include <stdint.h>

namespace MendocinoX6000 {

enum class Gate : uint8_t {
    MissingPciIdentity,
    UnexpectedPciClass,
    MissingIPDiscovery,
    UnsupportedGC,
    UnsupportedDCN,
    MissingBoardVbios,
    WaitingForX6000Accelerator,
    WaitingForX6000HWLibs,
    WaitingForX6000Framebuffer,
    EligibleForDriverBindingResearch,
};

// All evidence comes from the actual PCI device, validated board VBIOS,
// supplied and checksum-checked IP discovery, or observed Apple kext arrivals.
// This explicitly DOES NOT infer native matching from a kext being loaded.
constexpr Gate evaluate(const MendocinoBringup::State &state) {
    using namespace MendocinoBringup;
    if (!state.has(PciIdentity)) { return Gate::MissingPciIdentity; }
    if (!state.has(PciDisplayClass)) { return Gate::UnexpectedPciClass; }
    if (!state.has(IPDiscoveryValidated)) { return Gate::MissingIPDiscovery; }
    if (!state.has(GC1037Identified)) { return Gate::UnsupportedGC; }
    if (!state.has(DCN316Identified)) { return Gate::UnsupportedDCN; }
    if (!state.has(AtomVbiosPresent)) { return Gate::MissingBoardVbios; }
    if (!state.has(X6000AcceleratorSeen)) { return Gate::WaitingForX6000Accelerator; }
    if (!state.has(X6000HWLibsSeen)) { return Gate::WaitingForX6000HWLibs; }
    if (!state.has(X6000FramebufferSeen)) { return Gate::WaitingForX6000Framebuffer; }
    return Gate::EligibleForDriverBindingResearch;
}

constexpr bool candidateOnly(const MendocinoBringup::State &state) {
    return evaluate(state) == Gate::EligibleForDriverBindingResearch;
}

inline const char* name(Gate gate) {
    switch (gate) {
        case Gate::MissingPciIdentity: return "NO_MENDOCINO_PCI_ID";
        case Gate::UnexpectedPciClass: return "UNEXPECTED_PCI_CLASS";
        case Gate::MissingIPDiscovery: return "NO_VALIDATED_IP_DISCOVERY";
        case Gate::UnsupportedGC: return "GC_NOT_10_3_7";
        case Gate::UnsupportedDCN: return "DCN_NOT_3_1_6";
        case Gate::MissingBoardVbios: return "NO_VALID_BOARD_ATOM_VBIOS";
        case Gate::WaitingForX6000Accelerator: return "WAITING_FOR_NATIVE_X6000_ACCELERATOR";
        case Gate::WaitingForX6000HWLibs: return "WAITING_FOR_NATIVE_X6000_HWLIBS";
        case Gate::WaitingForX6000Framebuffer: return "WAITING_FOR_NATIVE_X6000_FRAMEBUFFER";
        case Gate::EligibleForDriverBindingResearch: return "CANDIDATE_ONLY_NO_NATIVE_BINDING_VERIFIED";
    }
    return "UNKNOWN";
}

// Never change these to true on the basis of the above gate alone.
constexpr bool driverBindingVerified(const MendocinoBringup::State &) { return false; }
constexpr bool accelerationVerified(const MendocinoBringup::State &state) {
    return state.accelerationVerified();
}
} // namespace MendocinoX6000
