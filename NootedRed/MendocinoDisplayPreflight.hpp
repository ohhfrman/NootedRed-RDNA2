// R6 Mendocino display *evidence* gate for DCN3.1.6 development.
// It does not program DMUB/DCN/AUX, select an eDP lane count or train a link.
#pragma once
#include <MendocinoAtomDisplay.hpp>
#include <MendocinoEDID.hpp>
#include <stdint.h>

namespace MendocinoDisplayPreflight {
enum class Status : uint8_t {
    MissingOrInvalidAtom,
    NoLCDTaggedPath,
    UnsupportedDceTable,
    NoDisplayPipes,
    MissingOrInvalidPanelTiming,
    TimingCandidate,
};
constexpr Status evaluate(const MendocinoAtomDisplay::Result &atom, const MendocinoEDID::Result &edid) {
    if (atom.status != MendocinoAtomDisplay::Status::OK) { return Status::MissingOrInvalidAtom; }
    // Tag LCD1 indicates a board display path, not a proven active eDP link.
    if (atom.firstWithTag(0x0002) == nullptr) { return Status::NoLCDTaggedPath; }
    if (!atom.dce44) { return Status::UnsupportedDceTable; }
    if (atom.maxPipes == 0 || atom.maxActivePipes == 0 || atom.maxActivePipes > atom.maxPipes) {
        return Status::NoDisplayPipes;
    }
    if (edid.status != MendocinoEDID::Status::OK || edid.timing.pixelClockKHz == 0 ||
        edid.timing.hActive == 0 || edid.timing.vActive == 0) {
        return Status::MissingOrInvalidPanelTiming;
    }
    return Status::TimingCandidate;
}
inline const char *name(Status status) {
    switch (status) {
        case Status::MissingOrInvalidAtom: return "NO_VALID_ATOM_DISPLAY_TABLES";
        case Status::NoLCDTaggedPath: return "NO_LCD_TAGGED_DISPLAY_PATH";
        case Status::UnsupportedDceTable: return "DCE_44_TABLE_NOT_AVAILABLE";
        case Status::NoDisplayPipes: return "INVALID_DISPLAY_PIPE_COUNTS";
        case Status::MissingOrInvalidPanelTiming: return "NO_VALIDATED_PANEL_EDID_TIMING";
        case Status::TimingCandidate: return "PANEL_TIMING_CANDIDATE_NO_EDP_LINK_TRAINING";
    }
    return "UNKNOWN";
}
} // namespace MendocinoDisplayPreflight
