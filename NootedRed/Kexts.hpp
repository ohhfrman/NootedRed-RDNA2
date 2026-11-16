// Kexts that patches will be applied on
//
// Copyright © 2025 ChefKiss. Licensed under the Thou Shalt Not Profit License version 1.5.
// See LICENSE for details.

#pragma once
#include <Headers/kern_patcher.hpp>

extern KernelPatcher::KextInfo kextRadeonX6000Framebuffer;
extern KernelPatcher::KextInfo kextRadeonX5000HWLibs;
extern KernelPatcher::KextInfo kextRadeonX5000;
extern KernelPatcher::KextInfo kextAppleBacklight;
extern KernelPatcher::KextInfo kextAppleMCCSControl;
extern KernelPatcher::KextInfo kextAGDP;
extern KernelPatcher::KextInfo kextAppleGFXHDA;

// Observation-only native RDNA2 components.
extern KernelPatcher::KextInfo kextRadeonX6000;
extern KernelPatcher::KextInfo kextRadeonX6000HWLibs;
