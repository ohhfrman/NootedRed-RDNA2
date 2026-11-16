// AMD IP Discovery preflight for Radeon 610M (GC10.3.7 / DCN3.1.6).
// Bounds-checked decoder of *supplied* AMD binary. No MMIO, DMA or firmware writes.
// Format references: Linux drivers/gpu/drm/amd/include/discovery.h and soc15_hw_ip.h (v6.6).
// Deliberately does not relicense or copy the Linux header.
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace MendocinoIP {
constexpr uint16_t kGC = 11, kDMU = 271, kSDMA0 = 42, kVCN = 12, kMP0 = 255, kMP1 = 1;
constexpr uint32_t kBinarySignature = 0x28211407;
constexpr uint32_t kIPSignature = 0x53445049;
constexpr size_t kMaxBytes = 65535;

enum class Status : uint8_t {
    OK, Empty, BadSignature, UnsupportedVersion, BadLength, BadChecksum,
    MissingIPTable, BadIPTable, BadDie, BadIPEntry, TooManyIPs
};
struct Block {
    uint16_t hwid{0};
    uint8_t instance{0};
    uint8_t major{0}, minor{0}, revision{0};
    uint8_t flags{0};
    uint8_t addressCount{0};
    uint8_t die{0};
    uint64_t base{0}; // register aperture base, not a physical or mapped pointer
    constexpr bool matches(uint8_t maj, uint8_t min, uint8_t rev) const {
        return major == maj && minor == min && revision == rev && (flags & 1) == 0;
    }
};
struct Result {
    Status status{Status::Empty};
    uint16_t count{0};
    Block blocks[96]{};
    bool found(uint16_t hw, uint8_t maj, uint8_t min, uint8_t rev) const {
        if (status != Status::OK) { return false; }
        for (unsigned i=0; i<count; ++i) {
            if (blocks[i].hwid == hw && blocks[i].matches(maj,min,rev)) { return true; }
        }
        return false;
    }
    const Block* get(uint16_t hw, uint8_t instance=0) const {
        if (status != Status::OK) { return nullptr; }
        for (unsigned i=0; i<count; ++i) {
            if (blocks[i].hwid == hw && blocks[i].instance == instance) { return &blocks[i]; }
        }
        return nullptr;
    }
    bool mendocinoGfxAndDisplay() const {
        return found(kGC,10,3,7) && found(kDMU,3,1,6);
    }
};
inline uint16_t u16(const uint8_t *p) { return static_cast<uint16_t>(p[0] | (uint16_t(p[1])<<8)); }
inline uint32_t u32(const uint8_t *p) { return uint32_t(u16(p)) | (uint32_t(u16(p+2))<<16); }
inline uint64_t u64(const uint8_t *p) { return uint64_t(u32(p)) | (uint64_t(u32(p+4))<<32); }
inline bool span(size_t off, size_t size, size_t limit) { return off <= limit && size <= limit-off; }
inline uint16_t sum16(const uint8_t* p, size_t n) {
    uint32_t sum=0; for (size_t i=0;i<n;++i) { sum += p[i]; } return static_cast<uint16_t>(sum);
}
// Covers discovery.bin v0/1 and v2 table directories, with strict table bounds.
inline void decodeInto(const uint8_t *data, size_t length, Result &out) {
    out.status = Status::Empty;
    out.count = 0;
    if (!data || length < 60) { return; }
    if (u32(data) != kBinarySignature) { out.status=Status::BadSignature; return; }
    const uint16_t major = u16(data+4);
    if (major > 2) { out.status=Status::UnsupportedVersion; return; }
    const size_t tableDir = major == 2 ? 16 : 12;
    const size_t tableCount = major == 2 ? u16(data+12) : 6;
    const size_t binarySize = u16(data+10);
    if (tableCount < 1 || tableCount > 6 || !span(tableDir,tableCount*8,length)
        || binarySize < tableDir+tableCount*8 || binarySize > length || binarySize > kMaxBytes) {
        out.status=Status::BadLength; return;
    }
    // AMD binary_checksum: sum of the binary starting immediately after checksum (offset 10).
    if (sum16(data+10,binarySize-10) != u16(data+8)) {
        out.status=Status::BadChecksum; return;
    }
    const uint16_t tableOffset=u16(data+tableDir);
    const uint16_t tableChecksum=u16(data+tableDir+2);
    const uint16_t directorySize=u16(data+tableDir+4);
    if (tableOffset==0) { out.status=Status::MissingIPTable; return; }
    if (!span(tableOffset,80,binarySize) || u32(data+tableOffset)!=kIPSignature) {
        out.status=Status::BadIPTable; return;
    }
    const uint16_t tableVersion=u16(data+tableOffset+4);
    const uint16_t tableSize=u16(data+tableOffset+6);
    if (tableVersion>4 || tableSize<80 || !span(tableOffset,tableSize,binarySize)
        || (directorySize && tableSize>directorySize)
        || sum16(data+tableOffset,tableSize)!=tableChecksum) {
        out.status=Status::BadIPTable; return;
    }
    const size_t tableEnd = size_t(tableOffset)+tableSize;
    const bool wide = tableVersion>=4 && (data[tableOffset+78] & 1);
    const uint16_t dies=u16(data+tableOffset+12);
    if (!dies || dies>16) { out.status=Status::BadDie; return; }
    for (unsigned die=0; die<dies; ++die) {
        // die_offset is an offset into the complete discovery binary, not the IP table.
        const size_t descriptor=tableOffset+14+die*4;
        const uint16_t dieId=u16(data+descriptor);
        const size_t dieOffset=u16(data+descriptor+2);
        if (!span(dieOffset,4,tableEnd) || dieOffset < tableOffset+80 ||
            dieId!=die || u16(data+dieOffset)!=dieId) {
            out.status=Status::BadDie; return;
        }
        const uint16_t ipCount=u16(data+dieOffset+2);
        if (ipCount>96 || ipCount>96-out.count) { out.status=Status::TooManyIPs; return; }
        size_t pos=dieOffset+4;
        for (unsigned j=0;j<ipCount;++j) {
            if (!span(pos,8,tableEnd)) { out.status=Status::BadIPEntry; return; }
            const uint8_t addresses=data[pos+3];
            const size_t bytesPerAddress=wide?8:4;
            if (addresses>16 || !span(pos+8,size_t(addresses)*bytesPerAddress,tableEnd)) {
                out.status=Status::BadIPEntry; return;
            }
            Block b;
            b.hwid=u16(data+pos);
            b.instance=data[pos+2];
            b.addressCount=addresses;
            b.major=data[pos+4]; b.minor=data[pos+5]; b.revision=data[pos+6];
            b.flags=(tableVersion<3) ? (data[pos+7]&0x0f) : 0; // v3+ field is subrevision/variant, not harvest
            b.die=static_cast<uint8_t>(die);
            b.base=addresses ? (wide ? u64(data+pos+8) : u32(data+pos+8)) : 0;
            out.blocks[out.count++]=b;
            pos+=8+size_t(addresses)*bytesPerAddress;
        }
    }
    out.status=Status::OK;
    return;
}

// Retain the original value-return API for host-side callers and existing tests.
inline Result decode(const uint8_t *data, size_t length) {
    Result out{};
    decodeInto(data, length, out);
    return out;
}
inline const char* statusName(Status s) {
    switch(s) {
        case Status::OK: return "IP_DISCOVERY_VALID";
        case Status::Empty: return "IP_DISCOVERY_ABSENT";
        case Status::BadSignature: return "IP_DISCOVERY_BAD_SIGNATURE";
        case Status::UnsupportedVersion: return "IP_DISCOVERY_UNSUPPORTED_VERSION";
        case Status::BadLength: return "IP_DISCOVERY_BAD_LENGTH";
        case Status::BadChecksum: return "IP_DISCOVERY_BAD_CHECKSUM";
        case Status::MissingIPTable: return "IP_DISCOVERY_NO_IP_TABLE";
        case Status::BadIPTable: return "IP_DISCOVERY_INVALID_IP_TABLE";
        case Status::BadDie: return "IP_DISCOVERY_INVALID_DIE";
        case Status::BadIPEntry: return "IP_DISCOVERY_INVALID_IP";
        case Status::TooManyIPs: return "IP_DISCOVERY_TOO_MANY_IPS";
    }
    return "IP_DISCOVERY_UNKNOWN";
}
} // namespace MendocinoIP
