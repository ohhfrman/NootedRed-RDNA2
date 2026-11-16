#include <MendocinoProviderMatch.hpp>
#include <cassert>
#include <cstdio>
int main() {
    using namespace MendocinoProviderMatch;
    assert(accepts(0x15061002, 0x030000C2, true));
    assert(accepts(0x15061002, 0x038000C2, true));
    assert(!accepts(0x15061002, 0x030000C2, false));
    assert(!accepts(0x15061002, 0x020000C2, true));
    assert(!accepts(0x15071002, 0x030000C2, true));
    assert(!accepts(0x150610DE, 0x030000C2, true));
    assert(!appleX6000ControllerBound());
    assert(!graphicsInitialized());
    std::puts("PASS: Stage3C opt-in exact PCI match; no X6000/Metal claims");
}
