#include "../NootedRed/MendocinoPciConfig.hpp"
#include <cassert>
#include <iostream>

int main()
{
    using namespace NRedHardware;
    constexpr PciSnapshot m{0x15061002, 0x03000001, 0x12341028, 0x0006, 0xFEDC000C, 0, 0xF0000000};
    static_assert(evaluate(m) == ProbeStatus::MendocinoPciVerified);
    static_assert(pciRevision(m.revisionClass) == 1);
    static_assert(pciSubVendor(m.subsystem) == 0x1028);
    static_assert(pciSubDevice(m.subsystem) == 0x1234);
    static_assert(decodeBAR(m.bar0).is64Bit);
    static_assert(decodeBAR(m.bar0).prefetchable);
    static_assert(decodeBAR(m.bar5).addressLow == 0xF0000000);
    static_assert(!decodeBAR(m.bar5).ioSpace);
    static_assert(evaluate({0x150610DE, 0x03000000, 0, 0, 0, 0, 0}) == ProbeStatus::NotMendocino);
    static_assert(evaluate({0x15061002, 0x02000000, 0, 0, 0, 0, 0}) == ProbeStatus::UnexpectedPciClass);
    static_assert(evaluate({0x16811002, 0x03000000, 0, 0, 0, 0, 0}) == ProbeStatus::NotMendocino);
    assert(decodeBAR(0x0000C001).ioSpace);
    assert(decodeBAR(0x0000C001).addressLow == 0xC000);
    std::cout << "PASS: read-only PCI snapshot classification and BAR decoding\n";
}
