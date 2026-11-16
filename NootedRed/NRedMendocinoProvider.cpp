// R6 Stage 3C: isolated, opt-in, nonexclusive IOKit PCI-provider attachment.
// NOT an Apple AMDRadeonX6000 controller or graphics initializer.
// Never open the GPU, map MMIO, program PCI config, enable bus mastering,
// patch native driver personalities, or claim eDP / GC / Metal readiness.
#include <MendocinoProviderMatch.hpp>
#include <IOKit/IOService.h>
#include <IOKit/pci/IOPCIDevice.h>
#include <libkern/c++/OSBoolean.h>
#include <libkern/c++/OSString.h>

class NRedMendocinoProvider final : public IOService {
    OSDeclareDefaultStructors(NRedMendocinoProvider)
public:
    IOService *probe(IOService *provider, SInt32 *score) override;
    bool start(IOService *provider) override;
};

OSDefineMetaClassAndStructors(NRedMendocinoProvider, IOService)

namespace {
constexpr const char *kOptInProperty = "NRed,EnableStage3CProvider";

bool matches(IOService *provider) {
    auto *gpu = OSDynamicCast(IOPCIDevice, provider);
    if (gpu == nullptr) { return false; }
    // An explicit OSBoolean true must already be injected by the owner.
    // The Info.plist IOPropertyMatch also requires this property. We check
    // again in probe/start to avoid relying solely on catalogue matching.
    const auto *consent = OSDynamicCast(OSBoolean, gpu->getProperty(kOptInProperty));
    const bool enabled = consent != nullptr && consent->isTrue();
    return MendocinoProviderMatch::accepts(
        gpu->configRead32(0x00), gpu->configRead32(0x08), enabled);
}
} // namespace

IOService *NRedMendocinoProvider::probe(IOService *provider, SInt32 *score) {
    if (!matches(provider)) { return nullptr; }
    return IOService::probe(provider, score);
}

bool NRedMendocinoProvider::start(IOService *provider) {
    if (!matches(provider) || !IOService::start(provider)) { return false; }

    // Publish evidence only on this service, not the underlying PCI provider.
    // No registry property implies Apple X6000 driver acceptance.
    auto *status = OSString::withCString("OWN_IOKIT_PROVIDER_ATTACHED_ONLY");
    if (status == nullptr) {
        IOService::stop(provider);
        return false;
    }
    const bool ok = setProperty("NRed,Stage3C-Status", status);
    status->release();
    if (!ok) {
        IOService::stop(provider);
        return false;
    }
    registerService();
    return true;
}
