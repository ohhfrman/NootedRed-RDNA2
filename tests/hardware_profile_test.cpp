#include "../NootedRed/HardwareProfile.hpp"
#include <cassert>
#include <iostream>

int main()
{
    using namespace NRedHardware;
    assert(classify(0x1002, 0x1506) == Architecture::MendocinoRDNA2);
    assert(classify(0x1002, 0x1636) == Architecture::Vega);
    assert(classify(0x1002, 0x15DD) == Architecture::Vega);
    assert(classify(0x1002, 0x1681) == Architecture::Unknown);
    assert(classify(0x1002, 0x164E) == Architecture::Unknown);
    assert(classify(0x10DE, 0x1506) == Architecture::Unknown);
    assert(mendocino.gc.packed() == 0x0A0307);
    assert(mendocino.dcn.packed() == 0x030106);
    assert(mendocino.sdma.packed() == 0x050207);
    assert(mendocino.vcn.packed() == 0x030101);
    assert(mendocino.mp0.packed() == 0x0D0008);
    assert(mendocino.mp1.packed() == 0x0D0008);
    std::cout << "PASS: Mendocino RDNA2 1002:1506 classification and IP profiles\n";
}
