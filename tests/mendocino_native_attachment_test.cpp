#include <MendocinoNativeAttachment.hpp>
#include <cassert>

int main() {
    using namespace MendocinoNativeAttachment;
    static_assert(classify(nullptr) == ServiceClass::Other);
    static_assert(classify("") == ServiceClass::Other);
    static_assert(classify("AMDRadeonX6000_AmdGpuWrangler") == ServiceClass::X6000GpuWrangler);
    static_assert(classify("AMDRadeonX6000_AmdRadeonControllerNavi10") == ServiceClass::X6000Navi10Controller);
    static_assert(classify("AMDRadeonX6000_AmdRadeonControllerNavi10_FAKE") == ServiceClass::Other);
    static_assert(classify("AMDRadeonX6000") == ServiceClass::Other);
    static_assert(evaluate({}) == Status::ScanUnavailable);
    static_assert(evaluate({true, false, false}) == Status::NoKnownX6000Child);
    static_assert(evaluate({true, true, false}) == Status::WranglerChildObserved);
    static_assert(evaluate({true, true, true}) == Status::Navi10ControllerChildObserved);
    static_assert(evaluate({false, true, true}) == Status::ScanUnavailable);
    static_assert(!hardwareBindingValidated({true, true, true}));
    assert(name(Status::Navi10ControllerChildObserved)[0] == 'N');
    return 0;
}
