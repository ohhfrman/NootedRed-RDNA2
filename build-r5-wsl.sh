#!/usr/bin/env bash
# Local only: does not commit, push, install, or load a kext.
set -euo pipefail
cd "$(dirname "$(readlink -f "$0")")"
export AppleForgePath="${AppleForgePath:-$HOME/AppleForge}"
LLVM_BIN="${LLVM_BIN:-$HOME/llvm-kext-build/bin}"
export PATH="$LLVM_BIN:$PATH"
for executable in clang clang++ ld64.lld llvm-readobj; do
    if [[ ! -x "$LLVM_BIN/$executable" ]]; then
        echo "Missing $LLVM_BIN/$executable" >&2
        exit 1
    fi
done
if [[ ! -f "$AppleForgePath/xmake/rules/AppleForge.lua" ]]; then
    echo "Missing AppleForge rules at $AppleForgePath/xmake/rules/AppleForge.lua" >&2
    exit 1
fi
printf 'Toolchain: '; "$LLVM_BIN/clang++" --version | head -n 1
printf 'Linker: '; "$LLVM_BIN/ld64.lld" --version | head -n 1
xmake f -c -p macosx -a x86_64 -m debug -y \
    --cc="$LLVM_BIN/clang" \
    --cxx="$LLVM_BIN/clang++" \
    --ld="$LLVM_BIN/clang++" \
    --ldflags="-Wl,-kext"
xmake -y -vD 2>&1 | tee "$HOME/nootedred-r5-build.log"
BIN="build/macosx/x86_64/debug/NootedRed.kext/Contents/MacOS/NootedRed"
if [[ ! -f "$BIN" ]]; then
    echo "Build returned success but expected binary not found: $BIN" >&2
    exit 2
fi
file "$BIN"
# This checks the actual linked KEXT even if Xmake uses cached object files.
if ! grep -aFq 'NRed,IPDiscovery-ParseStatus' "$BIN"; then
    echo 'ERROR: MendocinoProbe diagnostics are missing from the linked KEXT binary' >&2
    exit 4
fi
"$LLVM_BIN/llvm-readobj" --file-header "$BIN" | grep -E 'FileType:|Flags'
if ! "$LLVM_BIN/llvm-readobj" --file-header "$BIN" | grep -q 'FileType: KextBundle (0xB)'; then
    echo 'ERROR: Mach-O is NOT a KextBundle (0xB)' >&2
    exit 3
fi
echo 'R5 BUILD PASS: Mach-O KextBundle (0xB). Not hardware-validated; do not install yet.'
