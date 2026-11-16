// Radeon 610M bring-up: bounds-checked, read-only ACPI VFCT / ATOM VBIOS parsing.
// Table layout follows AMD's VFCT structures (atomfirmware.h), not a binary driver blob.
// This source does not install firmware or program GPU registers.
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace MendocinoVbios {

struct ImageView {
    const uint8_t* data{nullptr};
    size_t size{0};
    uint16_t vendor{0};
    uint16_t device{0};
    uint16_t subsystemVendor{0};
    uint16_t subsystemDevice{0};
    uint32_t bus{0};
    uint32_t slot{0};
    uint32_t function{0};
};

enum class Status : uint8_t {
    OK, NullOrShort, NotVFCT, BadACPIHeader, BadChecksum, BadOffset,
    TruncatedImage, InvalidImage, DeviceNotFound
};

struct Result {
    Status status{Status::NullOrShort};
    ImageView image{};
    unsigned scanned{0};
};

constexpr size_t kVFCTHeaderSize = 0x4C;
constexpr size_t kImageHeaderSize = 28;

inline uint16_t le16(const uint8_t* const p) {
    return uint16_t(p[0]) | uint16_t(uint16_t(p[1]) << 8);
}
inline uint32_t le32(const uint8_t* const p) {
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}

// AMD ATOM ROM includes the 55 AA signature, a ROM header pointer at 0x48,
// then an 'ATOM'/'MOTA' signature four bytes into that ROM header.
inline bool isAtomBios(const uint8_t* const p, const size_t n) {
    if (p == nullptr || n < 0x4A || p[0] != 0x55 || p[1] != 0xAA) { return false; }
    const size_t romHeader = le16(p + 0x48);
    if (romHeader > n || n - romHeader < 8) { return false; }
    const uint8_t* const sig = p + romHeader + 4;
    return (sig[0] == 'A' && sig[1] == 'T' && sig[2] == 'O' && sig[3] == 'M') ||
           (sig[0] == 'M' && sig[1] == 'O' && sig[2] == 'T' && sig[3] == 'A');
}

// Parse a *whole* ACPI VFCT table, not an arbitrary VBIOS/firmware image.
// Finds an image matching the real PCI identity rather than a donor's ID.
inline Result findVbios(const uint8_t* const data, const size_t available,
                        const uint16_t vendor = 0x1002, const uint16_t device = 0x1506) {
    Result result{};
    if (data == nullptr || available < kVFCTHeaderSize) { return result; }
    if (data[0] != 'V' || data[1] != 'F' || data[2] != 'C' || data[3] != 'T') {
        result.status = Status::NotVFCT;
        return result;
    }
    const uint32_t tableSize = le32(data + 4);
    if (tableSize < kVFCTHeaderSize || tableSize > available) {
        result.status = Status::BadACPIHeader;
        return result;
    }
    uint8_t sum = 0;
    for (size_t i = 0; i < tableSize; ++i) { sum = uint8_t(sum + data[i]); }
    if (sum != 0) { result.status = Status::BadChecksum; return result; }

    size_t offset = le32(data + 0x34);
    const uint32_t libOffset = le32(data + 0x38);
    const size_t end = (libOffset >= kVFCTHeaderSize && libOffset <= tableSize) ? libOffset : tableSize;
    if (offset < kVFCTHeaderSize || offset > end || end - offset < kImageHeaderSize) {
        result.status = Status::BadOffset;
        return result;
    }
    while (offset < end) {
        if (end - offset < kImageHeaderSize) { result.status = Status::TruncatedImage; return result; }
        const auto* const h = data + offset;
        const uint32_t imageSize = le32(h + 24);
        offset += kImageHeaderSize;
        // Subtraction instead of offset + imageSize prevents overflow.
        if (imageSize > end - offset) { result.status = Status::TruncatedImage; return result; }
        ++result.scanned;
        const uint16_t imgVendor = le16(h + 12);
        const uint16_t imgDevice = le16(h + 14);
        if (imgVendor == vendor && imgDevice == device) {
            const uint8_t* const blob = data + offset;
            if (!isAtomBios(blob, imageSize)) { result.status = Status::InvalidImage; return result; }
            result.image = {blob, imageSize, imgVendor, imgDevice, le16(h + 16), le16(h + 18),
                            le32(h), le32(h + 4), le32(h + 8)};
            result.status = Status::OK;
            return result;
        }
        if (imageSize == 0) { result.status = Status::InvalidImage; return result; }
        offset += imageSize;
    }
    result.status = Status::DeviceNotFound;
    return result;
}

inline const char* statusName(const Status status) {
    switch (status) {
        case Status::OK: return "ATOM_VBIOS_VERIFIED";
        case Status::NullOrShort: return "VFCT_MISSING_OR_SHORT";
        case Status::NotVFCT: return "NOT_ACPI_VFCT";
        case Status::BadACPIHeader: return "VFCT_INVALID_LENGTH";
        case Status::BadChecksum: return "VFCT_CHECKSUM_FAILED";
        case Status::BadOffset: return "VFCT_BAD_IMAGE_OFFSET";
        case Status::TruncatedImage: return "VFCT_IMAGE_TRUNCATED";
        case Status::InvalidImage: return "VFCT_ATOM_IMAGE_INVALID";
        case Status::DeviceNotFound: return "VFCT_DEVICE_1506_NOT_FOUND";
    }
    return "UNRECOGNIZED";
}

} // namespace MendocinoVbios
