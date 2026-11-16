// Stage 3D: bounded, read-only evaluation of real device-provided tables.
// Can be called before or after board firmware property handoff.
#include <MendocinoControllerSnapshot.hpp>
#include <MendocinoVbios.hpp>
#include <IOKit/pci/IOPCIDevice.h>
#include <IOKit/IOLib.h>
#include <libkern/c++/OSData.h>

// Snapshot the *actual* supplied inputs. Does not open the IOPCIDevice or
// configure BARs. A read-only diagnostic property is not a controller attach.
MendocinoControllerInputs::Status MendocinoControllerSnapshot::collect(IOPCIDevice *gpu) {
    const auto *rom = OSDynamicCast(OSData, gpu->getProperty("ATY,bin_image"));
    const bool validRom = rom != nullptr && MendocinoVbios::isAtomBios(
        static_cast<const uint8_t *>(rom->getBytesNoCopy()), rom->getLength());
    const auto atom = MendocinoAtomDisplay::inspect(
        validRom ? static_cast<const uint8_t *>(rom->getBytesNoCopy()) : nullptr,
        validRom ? rom->getLength() : 0);

    const auto *edidData = OSDynamicCast(OSData, gpu->getProperty("NRed,PanelEDID"));
    if (edidData == nullptr) {
        edidData = OSDynamicCast(OSData, gpu->getProperty("IODisplayEDID"));
    }
    const auto timing = MendocinoEDID::decode(
        edidData != nullptr ? static_cast<const uint8_t *>(edidData->getBytesNoCopy()) : nullptr,
        edidData != nullptr ? edidData->getLength() : 0);

    const auto *rawIP = OSDynamicCast(OSData, gpu->getProperty("NRed,AMD-IPDiscovery"));
    MendocinoIP::Result *ip = nullptr;
    if (rawIP != nullptr && rawIP->getLength() <= MendocinoIP::kMaxBytes) {
        // Result is several KB; never allocate it on the macOS kernel stack.
        ip = static_cast<MendocinoIP::Result *>(IOMalloc(sizeof(MendocinoIP::Result)));
        if (ip != nullptr) {
            MendocinoIP::decodeInto(
                static_cast<const uint8_t *>(rawIP->getBytesNoCopy()),
                rawIP->getLength(), *ip);
        }
    }
    const auto result = MendocinoControllerInputs::evaluate(
        gpu->configRead32(0x00), gpu->configRead32(0x08),
        validRom, atom, ip, timing);
    if (ip != nullptr) { IOFree(ip, sizeof(MendocinoIP::Result)); }
    return result;
}
