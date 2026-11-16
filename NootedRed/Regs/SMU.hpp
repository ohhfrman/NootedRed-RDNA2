// Register indices for the original Vega SMU code path.
// These are NOT valid as a Mendocino MP0/MP1 register map.
#pragma once
#include <IOKit/IOTypes.h>
constexpr UInt32 MP0_SMN_C2PMSG_58                     = 0x7A;
constexpr UInt32 MP0_SMN_C2PMSG_91                     = 0x9B;
constexpr UInt32 MP0_SMN_C2PMSG_100                    = 0xA4;
constexpr UInt32 MP1_SMN_C2PMSG_66                     = 0x282;
constexpr UInt32 MP1_SMN_C2PMSG_82                     = 0x292;
constexpr UInt32 MP1_SMN_C2PMSG_90                     = 0x29A;
constexpr UInt32 MP1_SMN_FPS_CNT                       = 0x2C4;
constexpr UInt32 MP1_FIRMWARE_FLAGS                    = 0x3010024;
constexpr UInt32 MP1_FIRMWARE_FLAGS_INTERRUPTS_ENABLED = 0x1;
