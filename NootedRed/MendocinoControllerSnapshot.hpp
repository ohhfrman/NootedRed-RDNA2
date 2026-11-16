// Shared Stage 3D read-only PCI and supplied firmware snapshot, no GPU init.
#pragma once
#include <MendocinoControllerInputs.hpp>
class IOPCIDevice;
namespace MendocinoControllerSnapshot {
MendocinoControllerInputs::Status collect(IOPCIDevice *gpu);
}
