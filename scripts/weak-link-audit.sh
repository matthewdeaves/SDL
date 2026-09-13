#!/bin/sh
# weak-link-audit.sh <binary> <expected-floor>
#
# Per-floor weak-link audit for a built PPC (or any Mach-O) SDL2 artifact.
# Checks the two things that actually cause a dyld-load crash on an older
# floor OS (SDL#2's failure mode): a binary whose own declared minimum OS
# version doesn't match the floor it's meant to ship on, and any undefined
# external symbol that the compiler did NOT mark as a weak import.
#
# What this catches: a hard (non-weak) reference to a symbol newer than the
# floor -- SDL#2's class of bug (_NSBackingPropertyOldScaleFactorKey hard-
# linked at a 10.7 build, needed 10.6). If the binary was compiled correctly
# against an SDK with proper availability annotations at the right
# -mmacosx-version-min, every symbol introduced after the floor is
# automatically weak-imported, so the interesting signal is (a) any MISMATCH
# between the binary's declared floor and the branch's floor, and (b) any
# hard undefined external at all, which is worth an eyeball even if most are
# legitimate (libSystem, pre-floor Foundation/AppKit calls).
#
# What this does NOT catch: a call into an Objective-C method the runtime
# doesn't implement (objc_msgSend to a missing selector), which is SDL#1's
# actual failure mode (alephone#37, [autorelease_pool drain] on Panther).
# That's a dynamic dispatch, not a linked symbol -- it never appears as an
# undefined external, weak or otherwise, so no static nm/otool check on the
# linked binary can see it. That needs either a real-hardware/emulated smoke
# test, or a static scan of every selector literal sent against a known
# per-OS class-dump, which is a separate, bigger tool. Do not claim this
# script would have caught alephone#37 -- it would not have.
#
# Usage:
#   weak-link-audit.sh <path-to-dylib-or-static-lib> <floor, e.g. 10.3>

set -eu

BIN="${1:?usage: weak-link-audit.sh <binary> <expected-floor>}"
FLOOR="${2:?usage: weak-link-audit.sh <binary> <expected-floor>}"

if [ ! -f "$BIN" ]; then
    echo "!! not found: $BIN" >&2
    exit 2
fi

echo "== weak-link audit: $BIN (expected floor $FLOOR) =="
echo

echo "-- declared minimum OS version (otool -l) --"
DECLARED=$(otool -l "$BIN" 2>/dev/null | awk '
    /LC_VERSION_MIN_MACOSX/ { want=1; next }
    /LC_BUILD_VERSION/      { want=1; next }
    want && /version/ { print $2; exit }
')
if [ -z "$DECLARED" ]; then
    echo "!! could not read a version-min load command from $BIN"
    echo "   (static archives carry no load commands of their own -- check"
    echo "    the linked binary that consumes this .a instead)"
else
    echo "declared: $DECLARED   branch floor: $FLOOR"
    if [ "$DECLARED" != "$FLOOR" ]; then
        echo "!! MISMATCH: this binary's own declared floor ($DECLARED) does not"
        echo "   match the branch floor ($FLOOR). This was SDL#2's actual root"
        echo "   cause (built at 10.7, needed 10.6) -- fix the build flags, not"
        echo "   the source."
    fi
fi
echo

echo "-- undefined external symbols: weak vs hard --"
TMP=$(mktemp)
trap 'rm -f "$TMP"' EXIT
nm -m "$BIN" 2>/dev/null | grep '(undefined)' > "$TMP" || true

if [ ! -s "$TMP" ]; then
    echo "(no undefined externals found -- fully static, or nm could not read this file)"
else
    WEAK=$(grep -c 'weak external' "$TMP" || true)
    HARD_LINES=$(grep -v 'weak external' "$TMP" | grep 'external' || true)
    HARD=$(printf '%s\n' "$HARD_LINES" | grep -c . || true)
    echo "weak (tolerated if missing at runtime): $WEAK"
    echo "hard (must exist on every OS this binary claims to support): $HARD"
    echo
    if [ "$HARD" -gt 0 ]; then
        echo "-- hard undefined externals (review: each must exist on $FLOOR) --"
        printf '%s\n' "$HARD_LINES" | sed 's/^/  /'
    fi
fi
