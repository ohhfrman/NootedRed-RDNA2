#include <MendocinoX6000Compatibility.hpp>
#include <assert.h>
#include <stdio.h>
#include <string.h>

using namespace MendocinoBringup;
using namespace MendocinoX6000;

int main() {
    State s{};
    assert(evaluate(s) == Gate::MissingPciIdentity);
    s.observe(PciIdentity);
    assert(evaluate(s) == Gate::UnexpectedPciClass);
    s.observe(PciDisplayClass);
    assert(evaluate(s) == Gate::MissingIPDiscovery);
    s.observe(IPDiscoveryValidated);
    assert(evaluate(s) == Gate::UnsupportedGC);
    s.observe(GC1037Identified);
    assert(evaluate(s) == Gate::UnsupportedDCN);
    s.observe(DCN316Identified);
    assert(evaluate(s) == Gate::MissingBoardVbios);
    s.observe(AtomVbiosPresent);
    assert(evaluate(s) == Gate::WaitingForX6000Accelerator);
    s.observe(X6000AcceleratorSeen);
    assert(evaluate(s) == Gate::WaitingForX6000HWLibs);
    s.observe(X6000HWLibsSeen);
    assert(evaluate(s) == Gate::WaitingForX6000Framebuffer);
    s.observe(X6000FramebufferSeen);
    assert(evaluate(s) == Gate::EligibleForDriverBindingResearch);
    assert(candidateOnly(s));
    assert(!driverBindingVerified(s));
    assert(!accelerationVerified(s));
    assert(strcmp(name(evaluate(s)), "CANDIDATE_ONLY_NO_NATIVE_BINDING_VERIFIED") == 0);
    // Even if the native Apple kexts are present, no valid IP report means no candidate.
    State noIp{};
    noIp.observe(PciIdentity); noIp.observe(PciDisplayClass);
    noIp.observe(AtomVbiosPresent); noIp.observe(X6000AcceleratorSeen);
    noIp.observe(X6000HWLibsSeen); noIp.observe(X6000FramebufferSeen);
    assert(evaluate(noIp) == Gate::MissingIPDiscovery);
    assert(!candidateOnly(noIp));
    // Nor can an unknown display version be waved through.
    State wrongDcn = s;
    wrongDcn.observations &= ~static_cast<uint32_t>(DCN316Identified);
    assert(evaluate(wrongDcn) == Gate::UnsupportedDCN);
    assert(!candidateOnly(wrongDcn));
    // Setting the candidate status must never set 'MetalVerified'.
    assert(!s.accelerationVerified());
    puts("PASS: R6 native X6000 gating (fail-closed, no false binding)");
    return 0;
}
