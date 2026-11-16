#include "../NootedRed/MendocinoIPDiscovery.hpp"
#include "../NootedRed/MendocinoEDID.hpp"
#include <assert.h>
#include <vector>
#include <stdint.h>
#include <stdio.h>
#include <algorithm>
using namespace MendocinoIP;

static void put16(std::vector<uint8_t>&v, size_t p, uint16_t x) { v[p]=uint8_t(x); v[p+1]=uint8_t(x>>8); }
static void put32(std::vector<uint8_t>&v, size_t p, uint32_t x) { put16(v,p,uint16_t(x)); put16(v,p+2,uint16_t(x>>16)); }
static void checksum(std::vector<uint8_t>&b, size_t tableOff, size_t len) {
    size_t entry = (MendocinoIP::u16(b.data()+4)==2) ? 18 : 14;
    put16(b,entry,0); // IP_DISCOVERY directory checksum
    put16(b,entry,MendocinoIP::sum16(b.data()+tableOff,len));
    put16(b,8,0);
    // Linux discovery uses checksum over bytes after binary_checksum field (offset 10).
    put16(b,8,MendocinoIP::sum16(b.data()+10,b.size()-10));
}
static std::vector<uint8_t> image(unsigned version, unsigned tableVersion=3, bool wide=false,
                                  bool rembrandt=false) {
    // One die; GC, DMU, SDMA0, MP1. Die data lives at absolute binary offset 256.
    std::vector<uint8_t>b(512,0);
    put32(b,0,kBinarySignature);
    put16(b,4,version);
    put16(b,10,uint16_t(b.size()));
    const size_t directory=(version==2?16:12);
    if (version==2) {put16(b,12,6);}
    put16(b,directory,128);
    put16(b,directory+4,256);
    put32(b,128,kIPSignature);
    put16(b,132,tableVersion);
    put16(b,134,256);
    put16(b,140,1); // num dies
    put16(b,142,0); // die zero
    put16(b,144,256); // absolute offset
    b[206] = wide ? 1 : 0; // v4 flags
    put16(b,256,0); put16(b,258,4);
    size_t at=260;
    struct IP { uint16_t id; uint8_t major, minor, revision; uint64_t addr; } ips[]={
        {kGC,10,3,uint8_t(rembrandt?3:7),0x7000000},
        {kDMU,3,1,uint8_t(rembrandt?2:6),0x5000000},
        {kSDMA0,5,2,uint8_t(rembrandt?3:7),0x3000000},
        {kMP1,13,0,uint8_t(rembrandt?3:8),0x1000000}
    };
    for (const auto& ip:ips) {
        put16(b,at,ip.id);
        b[at+2]=0;
        b[at+3]=1;
        b[at+4]=ip.major;b[at+5]=ip.minor;b[at+6]=ip.revision;
        put32(b,at+8,uint32_t(ip.addr));
        if(wide) put32(b,at+12,uint32_t(ip.addr>>32));
        at += wide ? 16 : 12;
    }
    checksum(b,128,256);
    return b;
}
static void assertIp() {
    for (auto version:{0,1,2}) {
        auto b=image(version);
        auto r=decode(b.data(),b.size());
        if (r.status!=Status::OK) { fprintf(stderr,"version=%u status=%s\n",version,statusName(r.status));}
        Result into{};
        into.status = Status::BadDie;
        into.count = 92;
        decodeInto(b.data(), b.size(), into);
        assert(into.status == r.status && into.count == r.count);
        assert(into.mendocinoGfxAndDisplay());
        decodeInto(nullptr, 0, into);
        assert(into.status == Status::Empty && into.count == 0);
        assert(r.status==Status::OK);
        assert(r.count==4);
        assert(r.mendocinoGfxAndDisplay());
        assert(r.found(kSDMA0,5,2,7));
        assert(r.get(kGC)->base==0x7000000ULL);
        assert(r.get(kGC)->addressCount==1);
        auto bad=b;bad[260+6]=4;
        assert(decode(bad.data(),bad.size()).status==Status::BadChecksum);
        bad=b;bad[0]=0;
        assert(decode(bad.data(),bad.size()).status==Status::BadSignature);
        bad=b;put16(bad,10,65000);
        assert(decode(bad.data(),bad.size()).status==Status::BadLength);
        bad=b;put16(bad,144,511);checksum(bad,128,256);
        assert(decode(bad.data(),bad.size()).status==Status::BadDie);
        bad=b;bad[260+3]=255;checksum(bad,128,256);
        assert(decode(bad.data(),bad.size()).status==Status::BadIPEntry);
        bad=b;put16(bad,258,100);checksum(bad,128,256);
        assert(decode(bad.data(),bad.size()).status==Status::TooManyIPs);
        bad=b;bad[130]=0;checksum(bad,128,256);
        assert(decode(bad.data(),bad.size()).status==Status::BadIPTable);
    }
    auto r2=decode(image(1,3,false,true).data(),512);
    assert(r2.status==Status::OK && !r2.mendocinoGfxAndDisplay());
    auto wide=image(2,4,true);
    auto r=decode(wide.data(),wide.size());
    assert(r.status==Status::OK && r.mendocinoGfxAndDisplay());
    wide[128+78]=0;checksum(wide,128,256);
    assert(!decode(wide.data(),wide.size()).mendocinoGfxAndDisplay());
    assert(decode(nullptr,0).status==Status::Empty);
}
static std::vector<uint8_t> makeEDID() {
    std::vector<uint8_t> v(128,0);
    const uint8_t hdr[]={0,255,255,255,255,255,255,0};
    std::copy(hdr,hdr+8,v.begin());
    // 1920x1080@60, 148.5 MHz; detailed timing w/ 280x45 blanking.
    size_t d=54;
    v[d]=0x02;v[d+1]=0x3a;
    v[d+2]=0x80;v[d+3]=0x18;v[d+4]=0x71;
    v[d+5]=0x38;v[d+6]=0x2d;v[d+7]=0x40;
    v[d+8]=88;v[d+9]=44;v[d+10]=0x45;v[d+11]=0;
    uint8_t c=0;for(uint8_t x:v)c=uint8_t(c+x);
    v[127]=uint8_t(-c);
    return v;
}
static void assertEDID() {
    auto edid=makeEDID();
    auto r=MendocinoEDID::decode(edid.data(),edid.size());
    assert(r.status==MendocinoEDID::Status::OK);
    assert(r.timing.hActive==1920 && r.timing.vActive==1080);
    assert(r.timing.refreshMilliHz==60000);
    edid[127]++;
    assert(MendocinoEDID::decode(edid.data(),edid.size()).status==MendocinoEDID::Status::BadChecksum);
    edid=makeEDID();edid[0]=0;
    assert(MendocinoEDID::decode(edid.data(),edid.size()).status==MendocinoEDID::Status::OK);
    edid=makeEDID();edid[0]=0x55;
    assert(MendocinoEDID::decode(edid.data(),edid.size()).status==MendocinoEDID::Status::BadHeader);
    assert(MendocinoEDID::decode(nullptr,0).status==MendocinoEDID::Status::Missing);
}
int main(){ assertIp();assertEDID();puts("Mendocino IP discovery v0/v1/v2 + v4 64-bit + EDID tests PASS"); }
