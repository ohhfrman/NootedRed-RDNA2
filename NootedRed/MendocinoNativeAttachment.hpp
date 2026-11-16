// R6 Stage 3B: evidence about real IOService children of PCI 1002:1506.
// An attached IOKit service is NOT evidence of GC/DCN initialization or Metal.
// No driver personalities are injected or PCI registers altered here.
#pragma once
#include <stdint.h>

namespace MendocinoNativeAttachment {

enum class ServiceClass : uint8_t {
    Other,
    X6000GpuWrangler,
    X6000Navi10Controller,
};

// Exact metaclass names only. Never infer attach from an IORegistry name,
// kext being loaded, or a user-controlled property on the PCI device.
constexpr bool same(const char *a, const char *b) {
    if (a == nullptr || b == nullptr) { return false; }
    while (*a != '\0' && *b != '\0') {
        if (*a != *b) { return false; }
        ++a; ++b;
    }
    return *a == *b;
}

constexpr ServiceClass classify(const char *metaclass) {
    if (same(metaclass, "AMDRadeonX6000_AmdGpuWrangler")) {
        return ServiceClass::X6000GpuWrangler;
    }
    if (same(metaclass, "AMDRadeonX6000_AmdRadeonControllerNavi10")) {
        return ServiceClass::X6000Navi10Controller;
    }
    return ServiceClass::Other;
}

enum class Status : uint8_t {
    ScanUnavailable,
    NoKnownX6000Child,
    WranglerChildObserved,
    Navi10ControllerChildObserved,
};

struct Evidence {
    bool traversalComplete{false};
    bool wranglerChild{false};
    bool navi10ControllerChild{false};
};

constexpr Status evaluate(const Evidence &evidence) {
    if (!evidence.traversalComplete) { return Status::ScanUnavailable; }
    if (evidence.navi10ControllerChild) { return Status::Navi10ControllerChildObserved; }
    if (evidence.wranglerChild) { return Status::WranglerChildObserved; }
    return Status::NoKnownX6000Child;
}

inline const char *name(Status status) {
    switch (status) {
        case Status::ScanUnavailable: return "SERVICE_TREE_SCAN_UNAVAILABLE";
        case Status::NoKnownX6000Child: return "NO_KNOWN_X6000_CHILD_OBSERVED";
        case Status::WranglerChildObserved: return "X6000_WRANGLER_CHILD_OBSERVED_ONLY";
        case Status::Navi10ControllerChildObserved:
            return "NAVI10_CONTROLLER_CHILD_OBSERVED_NOT_MENDOCINO_VALIDATED";
    }
    return "UNKNOWN";
}

// No 610M-native controller implementation has been validated. A Navi10
// child, even if genuinely attached, must never unlock hardware bring-up.
constexpr bool hardwareBindingValidated(const Evidence &) { return false; }

} // namespace MendocinoNativeAttachment
