#include "../NootedRed/MendocinoVbios.hpp"
#include "../NootedRed/MendocinoBringup.hpp"
#include <cassert>
#include <cstdint>
#include <vector>

using namespace MendocinoVbios;

static void put16(std::vector<uint8_t>& v, const size_t at, const uint16_t x) {
    v[at] = static_cast<uint8_t>(x);
    v[at + 1] = static_cast<uint8_t>(x >> 8);
}
static void put32(std::vector<uint8_t>& v, const size_t at, const uint32_t x) {
    put16(v, at, static_cast<uint16_t>(x));
    put16(v, at + 2, static_cast<uint16_t>(x >> 16));
}
static std::vector<uint8_t> bios() {
    std::vector<uint8_t> v(128, 0);
    v[0] = 0x55; v[1] = 0xAA;
    put16(v, 0x48, 0x50);
    v[0x54] = 'A'; v[0x55] = 'T'; v[0x56] = 'O'; v[0x57] = 'M';
    return v;
}
static void rec(std::vector<uint8_t>& table, const uint16_t vendor, const uint16_t device,
                const std::vector<uint8_t>& vbios) {
    const size_t base = table.size();
    table.resize(base + kImageHeaderSize + vbios.size(), 0);
    put32(table, base, 0);
    put32(table, base + 4, 1);
    put32(table, base + 8, 0);
    put16(table, base + 12, vendor);
    put16(table, base + 14, device);
    put16(table, base + 16, 0x1043);
    put16(table, base + 18, 0x1234);
    put32(table, base + 24, static_cast<uint32_t>(vbios.size()));
    for (size_t i = 0; i < vbios.size(); ++i) table[base + 28 + i] = vbios[i];
}
static void checksum(std::vector<uint8_t>& v) {
    put32(v, 4, static_cast<uint32_t>(v.size()));
    v[9] = 0;
    unsigned sum = 0;
    for (const auto b : v) sum += b;
    v[9] = static_cast<uint8_t>(-sum);
}
int main() {
    assert(!isAtomBios(nullptr, 0));
    auto rom = bios();
    assert(isAtomBios(rom.data(), rom.size()));
    rom[0x48] = 0x7F;
    assert(!isAtomBios(rom.data(), 127));
    rom = bios();
    std::vector<uint8_t> vfct(kVFCTHeaderSize, 0);
    vfct[0] = 'V'; vfct[1] = 'F'; vfct[2] = 'C'; vfct[3] = 'T';
    put32(vfct, 0x34, kVFCTHeaderSize);
    rec(vfct, 0x1002, 0x1681, rom); // A Rembrandt image MUST be ignored.
    rec(vfct, 0x1002, 0x1506, rom);
    checksum(vfct);
    auto result = findVbios(vfct.data(), vfct.size());
    assert(result.status == Status::OK);
    assert(result.scanned == 2);
    assert(result.image.device == 0x1506);
    assert(result.image.subsystemVendor == 0x1043);
    assert(result.image.size == 128);
    assert(result.image.data[0] == 0x55);
    auto noGpu = findVbios(vfct.data(), vfct.size(), 0x1002, 0x164E);
    assert(noGpu.status == Status::DeviceNotFound);
    vfct[9] ^= 0x01;
    assert(findVbios(vfct.data(), vfct.size()).status == Status::BadChecksum);
    vfct[9] ^= 0x01;
    put32(vfct, 0x34, 0xFFFFFFF0U);
    checksum(vfct);
    assert(findVbios(vfct.data(), vfct.size()).status == Status::BadOffset);
    put32(vfct, 0x34, kVFCTHeaderSize);
    put32(vfct, kVFCTHeaderSize + 24, 0xFFFFFFFFU);
    checksum(vfct);
    assert(findVbios(vfct.data(), vfct.size()).status == Status::TruncatedImage);
    assert(findVbios(nullptr, 0).status == Status::NullOrShort);

    MendocinoBringup::State state;
    state.observe(MendocinoBringup::PciIdentity);
    state.observe(MendocinoBringup::PciDisplayClass);
    state.observe(MendocinoBringup::X6000AcceleratorSeen);
    assert(!state.diagnosticsComplete());
    assert(!state.accelerationVerified());
    state.observe(MendocinoBringup::X6000FramebufferSeen);
    state.observe(MendocinoBringup::X6000HWLibsSeen);
    state.observe(MendocinoBringup::AtomVbiosPresent);
    assert(state.diagnosticsComplete());
    assert(!state.accelerationVerified()); // Seeing the binaries is NOT Metal.
}
