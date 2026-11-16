#include "../NootedRed/MendocinoAtomDisplay.hpp"
#include <cassert>
#include <cstdint>
#include <fstream>
#include <vector>

using namespace MendocinoAtomDisplay;
static void w16(uint8_t *b, size_t at, uint16_t v) { b[at]=uint8_t(v); b[at+1]=uint8_t(v>>8); }
int main(int argc, char** argv) {
    if (argc == 2) {
        // Optional real VFCT input for local hardware validation; never embed
        // the OEM firmware in the repository or patch.
        std::ifstream f(argv[1], std::ios::binary);
        assert(f.good());
        std::vector<uint8_t> vfct((std::istreambuf_iterator<char>(f)),
                                   std::istreambuf_iterator<char>());
        assert(vfct.size() > 0x4C);
        const size_t imageStart = size_t(vfct[0x34]) | (size_t(vfct[0x35]) << 8)
            | (size_t(vfct[0x36]) << 16) | (size_t(vfct[0x37]) << 24);
        assert(imageStart <= vfct.size() && vfct.size() - imageStart >= 28);
        const size_t romStart = imageStart + 28;
        const auto actual = inspect(vfct.data()+romStart,vfct.size()-romStart);
        assert(actual.status == Status::OK);
        assert(actual.pathCount == 6);
        assert(actual.firstWithTag(2) && actual.firstWithTag(2)->connector == 0x3114);
        assert(actual.firstWithTag(8) && actual.firstWithTag(8)->connector == 0x310C);
        assert(actual.dce44 && actual.maxPipes == 4 && actual.maxActivePipes == 1);
    }
    uint8_t b[1024]{};
    b[0]=0x55; b[1]=0xaa; w16(b,0x48,0x100);
    w16(b,0x100,0x2c); b[0x104]='A';b[0x105]='T';b[0x106]='O';b[0x107]='M';
    w16(b,0x120,0x200); w16(b,0x200,74); b[0x202]=2;b[0x203]=1;
    w16(b,0x200+4+22*2,0x300); w16(b,0x200+4+27*2,0x380);
    w16(b,0x300,40); b[0x302]=1; b[0x303]=4;
    w16(b,0x304,0xa);b[0x306]=2;
    w16(b,0x308,0x3114);w16(b,0x30a,0x28);w16(b,0x30c,0x211e);w16(b,0x310,0x2a);w16(b,0x314,0x0002);
    w16(b,0x318,0x310c);w16(b,0x31c,0x221e);w16(b,0x324,0x0008);
    // For record offsets used in this test, table size must include them.
    w16(b,0x300,48);
    w16(b,0x380,48);b[0x382]=4;b[0x383]=4;
    b[0x380+40]=1;b[0x380+41]=3;b[0x380+42]=4;b[0x380+43]=1;
    b[0x380+45]=5;b[0x380+46]=5;
    auto r=inspect(b,sizeof(b));
    assert(r.status==Status::OK && r.pathCount==2 && r.dce44);
    assert(r.firstWithTag(2) && r.firstWithTag(2)->connector==0x3114);
    assert(r.firstWithTag(8) && r.firstWithTag(8)->encoder==0x221e);
    assert(r.maxPipes==4 && r.maxActivePipes==1 && r.maxAuxPairs==5);
    b[0x306]=17;assert(inspect(b,sizeof(b)).status==Status::BadDisplayInfo);b[0x306]=2;
    w16(b,0x308+2,0xffff);assert(inspect(b,sizeof(b)).status==Status::BadDisplayInfo);
    w16(b,0x308+2,0x28);
    b[0x303]=5;assert(inspect(b,sizeof(b)).status==Status::UnsupportedDisplayInfo);b[0x303]=4;
    w16(b,0x380,12);assert(inspect(b,sizeof(b)).status==Status::BadDceInfo);
    return 0;
}
