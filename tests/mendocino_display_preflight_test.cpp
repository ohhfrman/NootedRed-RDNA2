#include <MendocinoDisplayPreflight.hpp>
#include <assert.h>
#include <stdio.h>

using namespace MendocinoDisplayPreflight;
int main() {
    MendocinoAtomDisplay::Result a{};
    MendocinoEDID::Result e{};
    assert(evaluate(a,e) == Status::MissingOrInvalidAtom);
    a.status = MendocinoAtomDisplay::Status::OK;
    a.pathCount = 1;
    a.paths[0].deviceTag = 0x0008;
    assert(evaluate(a,e) == Status::NoLCDTaggedPath);
    a.paths[0].deviceTag = 0x0002;
    assert(evaluate(a,e) == Status::UnsupportedDceTable);
    a.dce44 = true;
    assert(evaluate(a,e) == Status::NoDisplayPipes);
    a.maxPipes = 4;
    a.maxActivePipes = 1;
    assert(evaluate(a,e) == Status::MissingOrInvalidPanelTiming);
    e.status = MendocinoEDID::Status::OK;
    e.timing.hActive = 1920;
    e.timing.vActive = 1080;
    e.timing.pixelClockKHz = 138780;
    assert(evaluate(a,e) == Status::TimingCandidate);
    a.maxActivePipes = 5;
    assert(evaluate(a,e) == Status::NoDisplayPipes);
    puts("PASS: DCN3.1.6 panel timing preflight is fail-closed");
}
