#include <MendocinoControllerInputs.hpp>
#include <cassert>
#include <cstdio>

int main() {
    using namespace MendocinoControllerInputs;
    MendocinoAtomDisplay::Result atom{};
    atom.status = MendocinoAtomDisplay::Status::OK;
    atom.pathCount = 1;
    atom.paths[0].deviceTag = 0x0002;  // LCD1 only, not link proof.
    atom.dce44 = true;
    atom.maxPipes = 4;
    atom.maxActivePipes = 1;

    MendocinoEDID::Result edid{};
    edid.status = MendocinoEDID::Status::OK;
    edid.timing.hActive = 1920;
    edid.timing.vActive = 1080;
    edid.timing.pixelClockKHz = 148500;

    MendocinoIP::Result ip{};
    ip.status = MendocinoIP::Status::OK;
    ip.count = 4;
    ip.blocks[0].hwid = MendocinoIP::kGC;
    ip.blocks[0].major = 10; ip.blocks[0].minor = 3; ip.blocks[0].revision = 7;
    ip.blocks[1].hwid = MendocinoIP::kDMU;
    ip.blocks[1].major = 3; ip.blocks[1].minor = 1; ip.blocks[1].revision = 6;
    ip.blocks[2].hwid = MendocinoIP::kSDMA0;
    ip.blocks[2].major = 5; ip.blocks[2].minor = 2; ip.blocks[2].revision = 7;
    ip.blocks[3].hwid = MendocinoIP::kMP1;
    ip.blocks[3].major = 13; ip.blocks[3].minor = 0; ip.blocks[3].revision = 8;
    const auto run = [&]() {
        return evaluate(0x15061002U, 0x030000C2U, true, atom, &ip, edid);
    };
    assert(run() == Status::InputsValidatedOnly);
    assert(!nativeControllerInitialized(run()));
    assert(!metalAccelerationVerified(run()));
    assert(evaluate(0x16811002, 0x030000C2, true, atom, &ip, edid) == Status::WrongDevice);
    assert(evaluate(0x15061002, 0x020000C2, true, atom, &ip, edid) == Status::WrongClass);
    assert(evaluate(0x15061002, 0x030000C2, false, atom, &ip, edid) == Status::MissingOrInvalidBoardAtom);
    assert(evaluate(0x15061002, 0x030000C2, true, atom, nullptr, edid) == Status::MissingIPDiscovery);

    atom.pathCount = 0;
    assert(run() == Status::MissingLcdPath);
    atom.pathCount = 1;
    atom.dce44 = false;
    assert(run() == Status::UnsupportedDceInfo);
    atom.dce44 = true;
    atom.maxActivePipes = 5;
    assert(run() == Status::InvalidDisplayPipes);
    atom.maxActivePipes = 1;

    ip.status = MendocinoIP::Status::BadChecksum;
    assert(run() == Status::InvalidIPDiscovery);
    ip.status = MendocinoIP::Status::OK;
    ip.blocks[0].revision = 6;
    assert(run() == Status::UnsupportedGC);
    ip.blocks[0].revision = 7;
    ip.blocks[1].revision = 5;
    assert(run() == Status::UnsupportedDCN);
    ip.blocks[1].revision = 6;
    ip.blocks[2].revision = 6;
    assert(run() == Status::UnsupportedSDMA);
    ip.blocks[2].revision = 7;
    ip.blocks[3].revision = 7;
    assert(run() == Status::UnsupportedMP1);
    ip.blocks[3].revision = 8;
    edid.status = MendocinoEDID::Status::BadChecksum;
    assert(run() == Status::InvalidPanelTiming);

    std::puts("PASS: Stage3D input contract rejects missing/incorrect GC, DCN, SDMA, MP1, VBIOS and EDID");
}
