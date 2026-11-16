// R6 Stage 3D: read-only, fail-closed controller-input contract for Mendocino.
// This describes whether *inputs for future research* exist; it neither
// initializes Apple's X6000 controller nor grants permission to access MMIO.
#pragma once
#include <MendocinoAtomDisplay.hpp>
#include <MendocinoDisplayPreflight.hpp>
#include <MendocinoEDID.hpp>
#include <MendocinoIPDiscovery.hpp>
#include <MendocinoProviderMatch.hpp>
#include <stdint.h>

namespace MendocinoControllerInputs {

enum class Status : uint8_t {
    WrongDevice,
    WrongClass,
    MissingOrInvalidBoardAtom,
    InvalidAtomDisplay,
    MissingLcdPath,
    UnsupportedDceInfo,
    InvalidDisplayPipes,
    MissingIPDiscovery,
    InvalidIPDiscovery,
    UnsupportedGC,
    UnsupportedDCN,
    UnsupportedSDMA,
    UnsupportedMP1,
    InvalidPanelTiming,
    InputsValidatedOnly,
};

// IP discovery *must* be parsed from the board, not synthesized from the
// Mendocino marketing profile. A missing table is never an implicit match.
inline Status evaluate(uint32_t pciVendorDevice, uint32_t pciClassRevision,
                       bool boardAtomPresent,
                       const MendocinoAtomDisplay::Result &atom,
                       const MendocinoIP::Result *ip,
                       const MendocinoEDID::Result &edid) {
    if (pciVendorDevice != MendocinoProviderMatch::kVendorDevice) {
        return Status::WrongDevice;
    }
    if (!NRedHardware::isGraphicsClass(pciClassRevision)) {
        return Status::WrongClass;
    }
    if (!boardAtomPresent) { return Status::MissingOrInvalidBoardAtom; }
    const auto display = MendocinoDisplayPreflight::evaluate(atom, edid);
    switch (display) {
        case MendocinoDisplayPreflight::Status::MissingOrInvalidAtom:
            return Status::InvalidAtomDisplay;
        case MendocinoDisplayPreflight::Status::NoLCDTaggedPath:
            return Status::MissingLcdPath;
        case MendocinoDisplayPreflight::Status::UnsupportedDceTable:
            return Status::UnsupportedDceInfo;
        case MendocinoDisplayPreflight::Status::NoDisplayPipes:
            return Status::InvalidDisplayPipes;
        case MendocinoDisplayPreflight::Status::MissingOrInvalidPanelTiming:
        case MendocinoDisplayPreflight::Status::TimingCandidate:
            break; // Check the chip's independently supplied IP table first.
    }
    if (ip == nullptr) { return Status::MissingIPDiscovery; }
    if (ip->status != MendocinoIP::Status::OK) { return Status::InvalidIPDiscovery; }
    if (!ip->found(MendocinoIP::kGC, 10, 3, 7)) { return Status::UnsupportedGC; }
    if (!ip->found(MendocinoIP::kDMU, 3, 1, 6)) { return Status::UnsupportedDCN; }
    if (!ip->found(MendocinoIP::kSDMA0, 5, 2, 7)) { return Status::UnsupportedSDMA; }
    if (!ip->found(MendocinoIP::kMP1, 13, 0, 8)) { return Status::UnsupportedMP1; }
    if (display != MendocinoDisplayPreflight::Status::TimingCandidate) {
        return Status::InvalidPanelTiming;
    }
    return Status::InputsValidatedOnly;
}

inline const char *name(Status value) {
    switch (value) {
        case Status::WrongDevice: return "WRONG_PCI_DEVICE";
        case Status::WrongClass: return "NOT_A_DISPLAY_CLASS_DEVICE";
        case Status::MissingOrInvalidBoardAtom: return "BOARD_ATOM_ROM_MISSING_OR_INVALID";
        case Status::InvalidAtomDisplay: return "ATOM_DISPLAY_TOPOLOGY_INVALID";
        case Status::MissingLcdPath: return "LCD_PATH_MISSING";
        case Status::UnsupportedDceInfo: return "DCE_44_NOT_CONFIRMED";
        case Status::InvalidDisplayPipes: return "DISPLAY_PIPES_INVALID";
        case Status::MissingIPDiscovery: return "IP_DISCOVERY_NOT_PROVIDED";
        case Status::InvalidIPDiscovery: return "IP_DISCOVERY_INVALID";
        case Status::UnsupportedGC: return "GC_NOT_10_3_7";
        case Status::UnsupportedDCN: return "DCN_NOT_3_1_6";
        case Status::UnsupportedSDMA: return "SDMA_NOT_5_2_7";
        case Status::UnsupportedMP1: return "MP1_NOT_13_0_8";
        case Status::InvalidPanelTiming: return "PANEL_TIMING_INVALID_OR_MISSING";
        case Status::InputsValidatedOnly:
            return "CONTROLLER_INPUTS_VALIDATED_NO_NATIVE_INIT";
    }
    return "CONTROLLER_INPUT_STATUS_UNKNOWN";
}

// No Apple controller implementation or GC/DCN register contract exists yet.
// Validating data must NEVER transition the GPU to an initialized state.
constexpr bool nativeControllerInitialized(Status) { return false; }
constexpr bool metalAccelerationVerified(Status) { return false; }

} // namespace MendocinoControllerInputs
