#!/bin/sh
# weak-link-audit.sh <binary> <expected-floor>
#
# Per-floor weak-link audit for a built SDL2 artifact (dylib, executable, or
# static archive). Reports:
#   1. Whether the binary's own declared minimum OS version (where it has
#      one at all -- see LIMITS below) matches the floor it's meant to ship
#      on. This alone was SDL#2's actual root cause: built at 10.7, needed
#      10.6.
#   2. Every symbol this artifact needs from OUTSIDE itself -- i.e. every
#      undefined external symbol that is NOT satisfied by another object
#      file in the same archive/binary -- split into weak (dyld tolerates
#      it being missing) vs hard (must exist on every OS this binary claims
#      to support).
#
# For a static archive, step 2 matters far more than a naive `nm -m | grep
# undefined` suggests: most "undefined" symbols in an individual .o are
# resolved by ANOTHER .o in the same archive once everything is linked
# together (e.g. SDL_video.o calling something defined in SDL.o), so they
# are internal, not a real dependency on the floor OS. This script computes
# the archive-wide defined-symbol set and subtracts it, so what's left is
# what the linked binary will actually ask dyld to resolve.
#
# LIMITS, read before trusting a clean result:
#
# - Weak-vs-hard is a compiler feature (clang auto-weak-imports a symbol
#   whose SDK availability annotation is newer than -mmacosx-version-min).
#   The old gcc-4.0/PowerPC toolchain this fleet's Panther/Tiger floors are
#   built with does not appear to emit ANY weak markers at all -- measured
#   2026-09-13 against alephone's real sdl2-ppc-tiger103/lib/libSDL2.a: 0
#   "weak external" symbols out of 947 undefined ones, archive-wide. If this
#   script reports 0 weak and some hard count on a gcc/PPC build, that is
#   NOT evidence every hard symbol is actually available on the floor -- it
#   may just mean this toolchain never marks anything weak, full stop, and
#   the distinction this script draws for a clang-built artifact (see SDL#2,
#   which this script DOES meaningfully audit) does not hold here. Treat the
#   "truly external" list on a gcc/PPC build as a list to hand-check against
#   the floor SDK's headers, not as a pass/fail on its own.
# - Static archives generally carry no per-file load command with a minimum
#   OS version (check 1 will say so and stop there); check that on the
#   final linked binary that consumes the archive instead, if one exists.
# - This does NOT catch a call into an Objective-C method the runtime
#   doesn't implement (objc_msgSend to a missing selector) -- SDL#1's actual
#   failure mode (alephone#37, [autorelease_pool drain] on Panther). That's
#   dynamic dispatch, never a linked symbol, so no static nm/otool check on
#   a linked binary can see it. A different tool (real-hardware/emulated
#   smoke test, or a selector-literal scan against a per-OS class-dump)
#   would be needed for that class of bug. Do not claim this script would
#   have caught alephone#37 -- it would not have.
# - The optional SDK-header cross-check (below) is a text-level heuristic,
#   not a compiler: it greps for the symbol name and looks for an
#   AVAILABLE_MAC_OS_X_VERSION_10_N_AND_LATER / WEAK_IMPORT_ATTRIBUTE
#   annotation near the match. It can miss an annotation on a continuation
#   line it doesn't scan far enough to reach, and it cannot tell a genuine
#   declaration from an unrelated comment mentioning the same word. Treat
#   its RISK/NOT FOUND lines as "look at this by hand", and its OK lines as
#   "nothing suspicious turned up", not as a proof of safety.
#
# Usage:
#   weak-link-audit.sh <binary> <floor, e.g. 10.3> [floor-sdk-path]
#
# With the third argument (e.g. /Developer/SDKs/MacOSX10.3.9.sdk), each hard
# truly-external symbol is additionally cross-checked against that SDK's own
# headers: found with no newer-than-floor annotation, found but annotated
# for a later OS (a real risk on a toolchain that never weak-imports, like
# this fleet's gcc-4.0/PowerPC), or not found in the headers at all (also
# worth a direct look -- could be a private/undocumented API, or this
# script's grep just missed how it's declared).

set -eu

BIN="${1:?usage: weak-link-audit.sh <binary> <expected-floor> [floor-sdk-path]}"
FLOOR="${2:?usage: weak-link-audit.sh <binary> <expected-floor> [floor-sdk-path]}"
SDK_PATH="${3:-}"

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
    echo "!! no version-min load command found in $BIN"
    echo "   (a static archive carries none of its own -- check the final"
    echo "    linked binary instead. An old toolchain, e.g. gcc-4.0/PowerPC,"
    echo "    may not emit this load command on ANY artifact -- see the"
    echo "    LIMITS note at the top of this script before treating that as"
    echo "    a pass.)"
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

echo "-- symbols this artifact actually needs from outside itself --"
# Plain PID-based scratch files, not mktemp: some floor build hosts (e.g.
# macOS 10.7's mktemp) require a template argument and reject a bare call,
# and these are throwaway diagnostic files, not something that needs
# mktemp's collision-proofing.
DEFINED="/tmp/weak-link-audit.defined.$$"
UNDEF="/tmp/weak-link-audit.undef.$$"
trap 'rm -f "$DEFINED" "$UNDEF"' EXIT

NMOUT=$(nm -m "$BIN" 2>/dev/null) || true
printf '%s\n' "$NMOUT" | awk '!/\(undefined\)/ && /external/{print $NF}' | sort -u > "$DEFINED"
printf '%s\n' "$NMOUT" | awk '/\(undefined\)/ && /external/{print (/weak external/ ? "weak" : "hard"), $NF}' | sort -u -k2 > "$UNDEF"

# An archive member can be undefined-and-weak in one .o and undefined-and-
# hard in another for the same symbol name (rare, but nm sees each .o on its
# own); if both appear, prefer showing it as hard so a real per-floor risk
# is not hidden behind a weak match elsewhere.
TRULY_EXT=$(awk '{print $2}' "$UNDEF" | sort -u | while read -r sym; do
    grep -qxF "$sym" "$DEFINED" && continue
    if grep -q "^hard $sym\$" "$UNDEF"; then echo "hard $sym"; else echo "weak $sym"; fi
done)

WEAK_N=$(printf '%s\n' "$TRULY_EXT" | grep -c '^weak ' || true)
HARD_N=$(printf '%s\n' "$TRULY_EXT" | grep -c '^hard ' || true)
echo "weak (tolerated if missing at runtime): $WEAK_N"
echo "hard (must exist on every OS this binary claims to support): $HARD_N"
echo
if [ "$HARD_N" -gt 0 ]; then
    echo "-- hard, truly-external symbols (review each against the $FLOOR SDK) --"
    printf '%s\n' "$TRULY_EXT" | awk '/^hard /{print "  "$2}' | sort
fi

if [ -n "$SDK_PATH" ] && [ "$HARD_N" -gt 0 ]; then
    echo
    echo "-- cross-check against $SDK_PATH headers (floor $FLOOR) --"
    if [ ! -d "$SDK_PATH" ]; then
        echo "!! SDK path not found: $SDK_PATH" >&2
    else
        FLOOR_MINOR=$(printf '%s' "$FLOOR" | awk -F. '{print $2}')
        printf '%s\n' "$TRULY_EXT" | awk '/^hard /{print $2}' | sort | while read -r sym; do
            name=${sym#_}
            # First declaration match wins; framework bundles have many
            # symlink paths to the same physical header, not distinct
            # declarations, so more than one hit is expected duplication.
            #
            # --exclude='*.gch' and the trailing `|| true` are both load-
            # bearing, not cosmetic: a precompiled-header cache file (e.g.
            # usr/include/c++/4.0.0/.../stdc++.h.gch/O0g.gch, a serialized
            # compiler AST, not text) made this fleet's own grep abort with
            # "expression recursion level exceeded" partway through a real
            # run on mini-intel2 -- under `set -eu` that silently killed the
            # rest of the audit (68 of 191 symbols got a verdict, the other
            # 123 were never checked, and the caller saw exit 0 because a
            # trailing `rm -f` in the wrapper command masked the real
            # failure). Confirmed 2026-09-13. Skip .gch outright (it is
            # never a real declaration to find) and never let one symbol's
            # lookup take down every symbol after it.
            hit=$(grep -rn -w --exclude='*.gch' -- "$name" \
                    "$SDK_PATH/usr/include" \
                    "$SDK_PATH/System/Library/Frameworks" \
                    "$SDK_PATH/System/Library/PrivateFrameworks" \
                    "$SDK_PATH/Developer/Headers" 2>/dev/null | head -1) || true
            if [ -z "$hit" ]; then
                echo "  NOT FOUND in SDK headers: $sym  -- investigate directly, may be private/undocumented"
                continue
            fi
            file=$(printf '%s' "$hit" | cut -d: -f1)
            line=$(printf '%s' "$hit" | cut -d: -f2)
            # A few lines of context: the annotation macro is sometimes on
            # the declaration line, sometimes the line right after.
            ctx=$(sed -n "$((line>2?line-1:1)),$((line+2))p" "$file" 2>/dev/null) || true
            ver=$(printf '%s' "$ctx" | grep -o 'AVAILABLE_MAC_OS_X_VERSION_10_[0-9]*' | grep -o '[0-9]*$' | sort -rn | head -1)
            if [ -n "$ver" ] && [ "$ver" -gt "$FLOOR_MINOR" ]; then
                echo "  RISK: $sym annotated AVAILABLE_MAC_OS_X_VERSION_10_${ver}_AND_LATER, floor is $FLOOR ($file:$line)"
            elif printf '%s' "$ctx" | grep -q 'WEAK_IMPORT_ATTRIBUTE'; then
                echo "  found, marked weak_import in the SDK, but this toolchain emits no weak markers (see LIMITS): $sym ($file:$line)"
            else
                echo "  ok: $sym found, no newer-than-floor annotation nearby ($file:$line)"
            fi
        done
    fi
fi
