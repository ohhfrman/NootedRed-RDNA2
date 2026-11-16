// R6: read-only ATOMBIOS display topology preflight for Radeon 610M.
// The ATOM master list indices and v1.4/v4.4 table offsets follow AMD
// atomfirmware.h. No registers are accessed, and no display is initialized.
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace MendocinoAtomDisplay {

enum class Status : uint8_t {
    OK, MissingOrShort, NotAtom, BadRomHeader, BadMasterData,
    MissingDisplayInfo, BadDisplayInfo, UnsupportedDisplayInfo, BadDceInfo
};

struct Path {
    uint16_t connector{0};
    uint16_t encoder{0};
    uint16_t deviceTag{0};
    uint16_t connectorRecordOffset{0}; // relative to DisplayObjectInfo table
    uint16_t encoderRecordOffset{0};
};

struct Result {
    Status status{Status::MissingOrShort};
    uint16_t masterOffset{0};
    uint16_t displayOffset{0};
    uint16_t dceOffset{0};
    uint16_t supportedDevices{0};
    uint8_t pathCount{0};
    Path paths[16]{};
    bool dce44{false};
    uint8_t dcnMin{0};
    uint8_t dcnMax{0};
    uint8_t maxPipes{0};
    uint8_t maxActivePipes{0};
    uint8_t maxPhys{0};
    uint8_t maxAuxPairs{0};
    constexpr const Path* firstWithTag(uint16_t tag) const {
        for (unsigned i=0; i<pathCount; ++i)
            if ((paths[i].deviceTag & tag) != 0) return &paths[i];
        return nullptr;
    }
};

inline uint16_t u16(const uint8_t* p) {
    return static_cast<uint16_t>(uint16_t(p[0]) | (uint16_t(p[1]) << 8));
}
inline bool span(size_t off, size_t length, size_t limit) {
    return off <= limit && length <= limit - off;
}
inline bool atomSignature(const uint8_t* b, size_t n) {
    if (!b || n < 0x4A || b[0] != 0x55 || b[1] != 0xAA) return false;
    size_t rom = u16(b+0x48);
    if (!span(rom, 8, n)) return false;
    const uint8_t* s = b+rom+4;
    return (s[0]=='A' && s[1]=='T' && s[2]=='O' && s[3]=='M') ||
           (s[0]=='M' && s[1]=='O' && s[2]=='T' && s[3]=='A');
}

inline Result inspect(const uint8_t* b, size_t n) {
    Result out{};
    if (!b || n < 0x4A) return out;
    if (!atomSignature(b,n)) { out.status=Status::NotAtom; return out; }
    const size_t rom = u16(b+0x48);
    // v2.x ROM header: master data table offset resides at ROM+0x20.
    if (!span(rom, 0x24, n) || u16(b+rom) < 0x24) {
        out.status=Status::BadRomHeader; return out;
    }
    const size_t master = u16(b+rom+0x20);
    // Master table v2.1 has 35 * uint16 offsets, after 4-byte header.
    if (!span(master, 4+35*2, n) || u16(b+master) < 4+35*2 ||
        b[master+2] != 2 || b[master+3] != 1) {
        out.status=Status::BadMasterData; return out;
    }
    out.masterOffset = static_cast<uint16_t>(master);
    const size_t disp = u16(b+master+4+22*2);
    if (!disp) { out.status=Status::MissingDisplayInfo; return out; }
    if (!span(disp, 8, n)) { out.status=Status::BadDisplayInfo; return out; }
    const size_t dispSize = u16(b+disp);
    if (dispSize < 8 || !span(disp, dispSize, n)) {
        out.status=Status::BadDisplayInfo; return out;
    }
    if (b[disp+2] != 1 || b[disp+3] != 4) {
        out.status=Status::UnsupportedDisplayInfo; return out;
    }
    const uint8_t count = b[disp+6];
    if (count > 16 || size_t(count) > (dispSize-8)/16) {
        out.status=Status::BadDisplayInfo; return out;
    }
    out.displayOffset = static_cast<uint16_t>(disp);
    out.supportedDevices = u16(b+disp+4);
    out.pathCount=count;
    for (unsigned i=0;i<count;++i) {
        size_t off=disp+8+size_t(i)*16;
        Path p;
        p.connector=u16(b+off);
        p.connectorRecordOffset=u16(b+off+2);
        p.encoder=u16(b+off+4);
        p.encoderRecordOffset=u16(b+off+8);
        p.deviceTag=u16(b+off+12);
        // A record offset of zero means absent. Nonzero ones are relative
        // to this table (not to the beginning of the VBIOS image).
        if ((p.connectorRecordOffset && !span(p.connectorRecordOffset,2,dispSize)) ||
            (p.encoderRecordOffset && !span(p.encoderRecordOffset,2,dispSize))) {
            out.status=Status::BadDisplayInfo; return out;
        }
        out.paths[i]=p;
    }
    const size_t dce = u16(b+master+4+27*2);
    if (dce != 0) {
        if (!span(dce,4,n) || !span(dce,u16(b+dce),n) || u16(b+dce)<4) {
            out.status=Status::BadDceInfo; return out;
        }
        out.dceOffset=static_cast<uint16_t>(dce);
        // Interpret v4.4 fields only. Other versions stay unparsed.
        if (b[dce+2]==4 && b[dce+3]==4) {
            if (u16(b+dce)<48) { out.status=Status::BadDceInfo; return out; }
            out.dce44=true;
            out.dcnMin=b[dce+40]; out.dcnMax=b[dce+41];
            out.maxPipes=b[dce+42]; out.maxActivePipes=b[dce+43];
            out.maxPhys=b[dce+45]; out.maxAuxPairs=b[dce+46];
        }
    }
    out.status=Status::OK;
    return out;
}

inline const char* statusName(Status s) {
    switch(s) {
    case Status::OK: return "ATOM_DISPLAY_TABLES_VALID";
    case Status::MissingOrShort: return "ATOM_DISPLAY_MISSING_OR_SHORT";
    case Status::NotAtom: return "ATOM_DISPLAY_NOT_ATOM_ROM";
    case Status::BadRomHeader: return "ATOM_DISPLAY_BAD_ROM_HEADER";
    case Status::BadMasterData: return "ATOM_DISPLAY_BAD_MASTER_DATA";
    case Status::MissingDisplayInfo: return "ATOM_DISPLAY_INFO_ABSENT";
    case Status::BadDisplayInfo: return "ATOM_DISPLAY_INFO_INVALID";
    case Status::UnsupportedDisplayInfo: return "ATOM_DISPLAY_INFO_UNSUPPORTED_REVISION";
    case Status::BadDceInfo: return "ATOM_DCE_INFO_INVALID";
    }
    return "ATOM_DISPLAY_UNKNOWN";
}
} // namespace MendocinoAtomDisplay
