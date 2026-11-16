#!/bin/sh
# Diagnostic only: no system files changed. Run from macOS Terminal after boot.
set -eu
out="${1:-mendocino-macos-report.txt}"
{
    echo 'NootedRed Mendocino R3 macOS read-only report'
    echo 'Kernel and macOS version:'
    uname -sr
    sw_vers 2>&1 || true
    echo 'Loaded X6000 components (presence does not imply GPU binding):'
    kmutil showloaded 2>&1 | grep -Ei 'AMDRadeonX6000|AMDRadeonX6000HWLibs|NootedRed|as.vit9696.Lilu' || true
    echo 'X6000 component metadata, if present:'
    for kext in \
        /System/Library/Extensions/AMDRadeonX6000.kext \
        /System/Library/Extensions/AMDRadeonX6000Framebuffer.kext \
        /System/Library/Extensions/AMDRadeonX6000HWServices.kext/Contents/PlugIns/AMDRadeonX6000HWLibs.kext; do
        if test -f "$kext/Contents/Info.plist"; then
            echo "$kext"
            /usr/libexec/PlistBuddy -c 'Print :CFBundleIdentifier' "$kext/Contents/Info.plist" 2>&1 || true
            /usr/libexec/PlistBuddy -c 'Print :CFBundleVersion' "$kext/Contents/Info.plist" 2>&1 || true
        else
            echo "MISSING: $kext"
        fi
    done
    echo 'GPU profile:'
    system_profiler SPDisplaysDataType 2>&1 || true
    echo 'NRed IORegistry entries:'
    ioreg -p IOService -l -w0 2>&1 | grep 'NRed,' || true
    echo 'AMD graphics kexts loaded:'
    kmutil showloaded 2>&1 | grep -Ei 'AMDRadeonX[56]000|NootedRed|as.vit9696.Lilu' || true
    echo 'RDNA2 stage flags (read-only):'
    ioreg -p IOService -l -w0 2>&1 | grep -E 'NRed,(RDNA2-ObservedMask|VBIOS-Status|GC1037-Initialized|DCN316-Initialized|Metal-Verified)' || true
    echo 'RDNA2 plugin log messages (recent boot):'
    log show --last boot --style compact --predicate 'eventMessage CONTAINS "RDNA2" OR eventMessage CONTAINS "Mendocino"' 2>&1 || true
} > "$out"
printf 'Saved %s\n' "$out"
