// Standard EDID 1.x base block: safe preferred detailed timing decoding.
// Used for DCN3.1.6 panel timing preflight, not a framebuffer driver.
#pragma once
#include <stddef.h>
#include <stdint.h>
namespace MendocinoEDID {
enum class Status : uint8_t { OK, Missing, BadHeader, BadChecksum, NoTiming, InvalidTiming };
struct Timing {
    uint32_t pixelClockKHz{0};
    uint16_t hActive{0}, hBlank{0}, vActive{0}, vBlank{0};
    uint16_t hSyncStart{0}, hSyncWidth{0}, vSyncStart{0}, vSyncWidth{0};
    uint32_t refreshMilliHz{0};
};
struct Result { Status status{Status::Missing}; Timing timing{}; };
inline Result decode(const uint8_t* p, size_t n) {
    Result r{};
    if (!p || n<128) { return r; }
    const uint8_t header[8]={0,255,255,255,255,255,255,0};
    for (size_t i=0;i<8;++i) if(p[i]!=header[i]) { r.status=Status::BadHeader; return r; }
    uint8_t sum=0; for (size_t i=0;i<128;++i) sum=static_cast<uint8_t>(sum+p[i]);
    if(sum) { r.status=Status::BadChecksum; return r; }
    for (unsigned i=0;i<4;++i) {
        const uint8_t* d=p+54+18*i;
        uint32_t clock=(uint32_t(d[0]) | (uint32_t(d[1])<<8))*10;
        if (!clock) { continue; }
        auto& t=r.timing;
        t.pixelClockKHz=clock;
        t.hActive=uint16_t(d[2]) | uint16_t((d[4]&0xf0)<<4);
        t.hBlank=uint16_t(d[3]) | uint16_t((d[4]&0x0f)<<8);
        t.vActive=uint16_t(d[5]) | uint16_t((d[7]&0xf0)<<4);
        t.vBlank=uint16_t(d[6]) | uint16_t((d[7]&0x0f)<<8);
        t.hSyncStart=uint16_t(d[8]) | uint16_t((d[11]&0xc0)<<2);
        t.hSyncWidth=uint16_t(d[9]) | uint16_t((d[11]&0x30)<<4);
        t.vSyncStart=uint16_t((d[10]>>4)&0x0f) | uint16_t((d[11]&0x0c)<<2);
        t.vSyncWidth=uint16_t(d[10]&0x0f) | uint16_t((d[11]&0x03)<<4);
        const uint32_t hTotal=uint32_t(t.hActive)+t.hBlank;
        const uint32_t vTotal=uint32_t(t.vActive)+t.vBlank;
        if (!clock || t.hActive<320 || t.vActive<200 || !t.hBlank || !t.vBlank ||
            hTotal>8192 || vTotal>8192 || t.hSyncStart+t.hSyncWidth>t.hBlank ||
            t.vSyncStart+t.vSyncWidth>t.vBlank) {
            r.status=Status::InvalidTiming; return r;
        }
        t.refreshMilliHz=static_cast<uint32_t>((uint64_t(clock)*1000000ULL)/(uint64_t(hTotal)*vTotal));
        if (t.refreshMilliHz<10000 || t.refreshMilliHz>360000) {
            r.status=Status::InvalidTiming; return r;
        }
        r.status=Status::OK; return r;
    }
    r.status=Status::NoTiming; return r;
}
inline const char* statusName(Status status) {
    switch(status) {
        case Status::OK: return "EDID_PREFERRED_TIMING_VALID";
        case Status::Missing: return "EDID_MISSING";
        case Status::BadHeader: return "EDID_BAD_HEADER";
        case Status::BadChecksum: return "EDID_BAD_CHECKSUM";
        case Status::NoTiming: return "EDID_NO_DETAILED_TIMING";
        case Status::InvalidTiming: return "EDID_INVALID_TIMING";
    }
    return "EDID_UNKNOWN";
}
} // namespace MendocinoEDID
