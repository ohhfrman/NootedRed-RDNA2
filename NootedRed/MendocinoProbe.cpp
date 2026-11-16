// Read-only PCI enumeration of Radeon 610M; no GC/SDMA/SMU/MMIO programming.
#include <HardwareProfile.hpp>
#include <MendocinoPciConfig.hpp>
#include <MendocinoVbios.hpp>
#include <MendocinoIPDiscovery.hpp>
#include <MendocinoEDID.hpp>
#include <MendocinoBringup.hpp>
#include <MendocinoProbe.hpp>
#include <IOKit/pci/IOPCIDevice.h>
#include <IOKit/IOLib.h>
#include <libkern/c++/OSString.h>
#include <libkern/c++/OSData.h>

namespace {
// The PCI device remains owned by IOKit. This is only used while the plugin is loaded.
IOPCIDevice* observedGpu = nullptr;
MendocinoBringup::State state{};
// A native kext can be observed before Lilu identifies the GPU.
// These flags record only that the binaries loaded, not Radeon 610M binding.
uint32_t loadedComponentBits = 0;


void updateStageProperties() {
    if (observedGpu == nullptr) { return; }
    observedGpu->setProperty("NRed,RDNA2-ObservedMask", static_cast<unsigned long long>(state.observations), 32);
    observedGpu->setProperty("NRed,DCN316-Initialized", static_cast<unsigned long long>(state.has(MendocinoBringup::DCN316Initialized)), 32);
    observedGpu->setProperty("NRed,GC1037-Initialized", static_cast<unsigned long long>(state.has(MendocinoBringup::GC1037Initialized)), 32);
    observedGpu->setProperty("NRed,Metal-Verified", static_cast<unsigned long long>(state.accelerationVerified()), 32);
    observedGpu->setProperty("NRed,GC1037-Identified", static_cast<unsigned long long>(state.has(MendocinoBringup::GC1037Identified)), 32);
    observedGpu->setProperty("NRed,DCN316-Identified", static_cast<unsigned long long>(state.has(MendocinoBringup::DCN316Identified)), 32);
    observedGpu->setProperty("NRed,IPDiscovery-Validated", static_cast<unsigned long long>(state.has(MendocinoBringup::IPDiscoveryValidated)), 32);
    observedGpu->setProperty("NRed,BoardProfile-Confirmed", static_cast<unsigned long long>(state.hardwareProfileConfirmed()), 32);
}

void setString(IOPCIDevice* const gpu, const char* const key, const char* const value)
{
    auto* str = OSString::withCString(value);
    if (str != nullptr) {
        gpu->setProperty(key, str);
        str->release();
    }
}

void set32(IOPCIDevice* const gpu, const char* const key, const uint32_t value)
{
    gpu->setProperty(key, static_cast<unsigned long long>(value), 32);
}
}

void MendocinoProbe::publish(IOPCIDevice* const gpu, const unsigned int pciRevision)
{
    if (gpu == nullptr) { return; }
    // These are immutable PCI configuration reads; do not enable BARs or bus mastering.
    const NRedHardware::PciSnapshot pci{
        gpu->configRead32(0x00),
        gpu->configRead32(0x08),
        gpu->configRead32(0x2C),
        gpu->configRead16(0x04),
        gpu->configRead32(0x10),
        gpu->configRead32(0x18),
        gpu->configRead32(0x24),
    };
    const auto status = NRedHardware::evaluate(pci);
    if (status == NRedHardware::ProbeStatus::NotMendocino) { return; }

    setString(gpu, "NRed,Architecture", "RDNA2 Mendocino");
    setString(gpu, "NRed,Model", "AMD Radeon 610M");
    observedGpu = gpu;
    state = {};
    state.observations |= loadedComponentBits;
    state.observe(MendocinoBringup::PciIdentity);
    if (status == NRedHardware::ProbeStatus::MendocinoPciVerified) {
        state.observe(MendocinoBringup::PciDisplayClass);
    }
    setString(gpu, "NRed,PortStatus", "R5_RDNA2_GC_DCN_IP_PREFLIGHT_NO_ACCELERATION");
    // Board firmware handoff: optional *input* from OpenCore/other trusted loader,
    // not a native ACPI table read. No ROM donor and no GPU programming.
    // Refuse to replace a pre-existing valid VBIOS. This is the correct
    // ATY,bin_image property format used by the macOS AMD stack.
    auto* rom = OSDynamicCast(OSData, gpu->getProperty("ATY,bin_image"));
    bool atomOK = rom != nullptr &&
        MendocinoVbios::isAtomBios(static_cast<const uint8_t*>(rom->getBytesNoCopy()), rom->getLength());
    setString(gpu, "NRed,VFCT-Handoff", "NOT_PERFORMED");
    auto* const vfct = OSDynamicCast(OSData, gpu->getProperty("NRed,ACPI-VFCT"));
    if (!atomOK && vfct != nullptr) {
        const auto extracted = MendocinoVbios::findVbios(
            static_cast<const uint8_t*>(vfct->getBytesNoCopy()), vfct->getLength());
        setString(gpu, "NRed,VFCT-ParseStatus", MendocinoVbios::statusName(extracted.status));
        if (extracted.status == MendocinoVbios::Status::OK && extracted.image.size <= 512 * 1024) {
            auto* const boardImage = OSData::withBytes(extracted.image.data, static_cast<unsigned int>(extracted.image.size));
            if (boardImage != nullptr) {
                const bool installed = gpu->setProperty("ATY,bin_image", boardImage);
                boardImage->release();
                if (installed) {
                    rom = OSDynamicCast(OSData, gpu->getProperty("ATY,bin_image"));
                    atomOK = rom != nullptr && MendocinoVbios::isAtomBios(
                        static_cast<const uint8_t*>(rom->getBytesNoCopy()),rom->getLength());
                    if (atomOK) {
                        state.observe(MendocinoBringup::VbiosHandoffComplete);
                        setString(gpu, "NRed,VFCT-Handoff", "BOARD_1506_ROM_INSTALLED_IN_IOREGISTRY");
                    }
                }
            }
        }
    }
    if (atomOK) { state.observe(MendocinoBringup::AtomVbiosPresent); }
    setString(gpu, "NRed,VBIOS-Status", atomOK ? "PRESENT_ATOM_VALID" : "MISSING_OR_INVALID");
    set32(gpu, "NRed,VBIOS-Bytes", rom != nullptr ? static_cast<uint32_t>(rom->getLength()) : 0);

    // GC10.3.7 / DCN3.1.6 version AND register-base preflight. The binary must
    // be supplied as NRed,AMD-IPDiscovery by a loader; we cannot safely infer
    // the AMD PSP discovery memory address on a random notebook.
    auto* const rawIP = OSDynamicCast(OSData, gpu->getProperty("NRed,AMD-IPDiscovery"));
    // Result holds 96 IP blocks (~2.3 KiB). Avoid keeping it on the kernel stack.
    // decodeInto resets its status/count before reading any of the entries.
    void* const ipStorage = IOMalloc(sizeof(MendocinoIP::Result));
    if (ipStorage == nullptr) {
        setString(gpu, "NRed,IPDiscovery-ParseStatus", "IP_DISCOVERY_OUT_OF_MEMORY");
    } else {
        auto* const ip = static_cast<MendocinoIP::Result*>(ipStorage);
        MendocinoIP::decodeInto(rawIP != nullptr
            ? static_cast<const uint8_t*>(rawIP->getBytesNoCopy()) : nullptr,
            rawIP != nullptr ? rawIP->getLength() : 0, *ip);
        setString(gpu, "NRed,IPDiscovery-ParseStatus", MendocinoIP::statusName(ip->status));
        if (ip->status == MendocinoIP::Status::OK) {
            state.observe(MendocinoBringup::IPDiscoveryValidated);
            const auto* const gc = ip->get(MendocinoIP::kGC);
            const auto* const dcn = ip->get(MendocinoIP::kDMU);
            if (ip->found(MendocinoIP::kGC,10,3,7)) { state.observe(MendocinoBringup::GC1037Identified); }
            if (ip->found(MendocinoIP::kDMU,3,1,6)) { state.observe(MendocinoBringup::DCN316Identified); }
            set32(gpu, "NRed,IPDiscovery-RecordCount", ip->count);
            set32(gpu, "NRed,SDMA527-Identified", ip->found(MendocinoIP::kSDMA0,5,2,7));
            set32(gpu, "NRed,MP1-1308-Identified", ip->found(MendocinoIP::kMP1,13,0,8));
            if (gc != nullptr) {
                set32(gpu, "NRed,GC-DiscoveredIP", uint32_t(gc->major)<<16 | uint32_t(gc->minor)<<8 | gc->revision);
                set32(gpu, "NRed,GC-Base0-Lo", uint32_t(gc->base));
                set32(gpu, "NRed,GC-Base0-Hi", uint32_t(gc->base>>32));
            }
            if (dcn != nullptr) {
                set32(gpu, "NRed,DCN-DiscoveredIP", uint32_t(dcn->major)<<16 | uint32_t(dcn->minor)<<8 | dcn->revision);
                set32(gpu, "NRed,DMU-Base0-Lo", uint32_t(dcn->base));
                set32(gpu, "NRed,DMU-Base0-Hi", uint32_t(dcn->base>>32));
            }
        }
        IOFree(ipStorage, sizeof(MendocinoIP::Result));
    }
    // EDID is not always a PCI-device property: a loader may supply an EDID
    // under NRed,PanelEDID; no claim is made about eDP link training.
    auto* panel = OSDynamicCast(OSData, gpu->getProperty("NRed,PanelEDID"));
    if (panel == nullptr) { panel = OSDynamicCast(OSData, gpu->getProperty("IODisplayEDID")); }
    const auto edid = MendocinoEDID::decode(panel != nullptr
        ? static_cast<const uint8_t*>(panel->getBytesNoCopy()) : nullptr,
        panel != nullptr ? panel->getLength() : 0);
    setString(gpu, "NRed,EDID-ParseStatus", MendocinoEDID::statusName(edid.status));
    if (edid.status == MendocinoEDID::Status::OK) {
        state.observe(MendocinoBringup::EDIDTimingValidated);
        set32(gpu, "NRed,Panel-HActive", edid.timing.hActive);
        set32(gpu, "NRed,Panel-VActive", edid.timing.vActive);
        set32(gpu, "NRed,Panel-PixelClockKHz", edid.timing.pixelClockKHz);
        set32(gpu, "NRed,Panel-RefreshMilliHz", edid.timing.refreshMilliHz);
    }
    updateStageProperties();
    setString(gpu, "NRed,ProbeResult", status == NRedHardware::ProbeStatus::MendocinoPciVerified
                                        ? "MENDOCINO_PCI_CLASS_VERIFIED" : "UNEXPECTED_PCI_CLASS");
    set32(gpu, "NRed,PCI-ID", NRedHardware::pciDevice(pci.vendorDevice));
    set32(gpu, "NRed,PCI-Revision", NRedHardware::pciRevision(pci.revisionClass));
    set32(gpu, "NRed,RevisionMatchesLilu", static_cast<uint8_t>(pciRevision) == NRedHardware::pciRevision(pci.revisionClass));
    set32(gpu, "NRed,PCI-ClassRevisionRaw", pci.revisionClass);
    set32(gpu, "NRed,PCI-BaseClass", NRedHardware::pciBaseClass(pci.revisionClass));
    set32(gpu, "NRed,PCI-Subclass", NRedHardware::pciSubclass(pci.revisionClass));
    set32(gpu, "NRed,PCI-Command", pci.command);
    set32(gpu, "NRed,SubsystemVendorID", NRedHardware::pciSubVendor(pci.subsystem));
    set32(gpu, "NRed,SubsystemDeviceID", NRedHardware::pciSubDevice(pci.subsystem));
    // Raw BAR fields are deliberately NOT dereferenced or probed for size.
    set32(gpu, "NRed,PCI-BAR0-Raw", pci.bar0);
    set32(gpu, "NRed,PCI-BAR2-Raw", pci.bar2);
    set32(gpu, "NRed,PCI-BAR5-Raw", pci.bar5);
    set32(gpu, "NRed,BAR5-IsIO", NRedHardware::decodeBAR(pci.bar5).ioSpace);
    set32(gpu, "NRed,GC-IP", NRedHardware::mendocino.gc.packed());
    set32(gpu, "NRed,DCN-IP", NRedHardware::mendocino.dcn.packed());
    set32(gpu, "NRed,SDMA-IP", NRedHardware::mendocino.sdma.packed());
    set32(gpu, "NRed,VCN-IP", NRedHardware::mendocino.vcn.packed());
    set32(gpu, "NRed,MP0-IP", NRedHardware::mendocino.mp0.packed());
    set32(gpu, "NRed,MP1-IP", NRedHardware::mendocino.mp1.packed());
    // Note: published IP values describe the known SKU, not hardware-discovered IP blocks.
    setString(gpu, "NRed,IPVersionSource", "Linux AMD Mendocino product table (not live IP discovery)");
}

void MendocinoProbe::noteX6000Component(const MendocinoProbe::Component component)
{
    MendocinoBringup::Bit bit;
    switch (component) {
        case Component::Framebuffer: bit = MendocinoBringup::X6000FramebufferSeen; break;
        case Component::Accelerator: bit = MendocinoBringup::X6000AcceleratorSeen; break;
        case Component::HWLibs: bit = MendocinoBringup::X6000HWLibsSeen; break;
        default: return;
    }
    loadedComponentBits |= static_cast<uint32_t>(bit);
    if (observedGpu == nullptr) { return; }
    state.observe(bit);
    updateStageProperties();
}
