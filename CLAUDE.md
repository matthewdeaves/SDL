Generative AI, including large language models (LLMs), should not be used in
any way when contributing to SDL.

We want our code to be art. We want to interact with real humans. Please don't
submit AI-generated comments or code in bug reports or pull requests. We
understand some people consider AI to be a useful tool, but we want to connect
with you, not your computer.

Any pull request to this project will ask you to confirm that you are the
author and that you are contributing your changes under the zlib license.

---

## This fork (matthewdeaves/SDL): retro floor branches

This is a personal fork used only to build and maintain a private fleet of
Mac OS X ports (Half-Life, Quake II, Quake III, QuakeSpasm, Aleph One). It is
never used to file, comment on, or send pull requests to libsdl-org/SDL — the
policy above is about upstream contribution and doesn't apply to work kept
entirely on this fork. Nothing here is AI-authored code contributed to the
SDL project; it is AI-assisted maintenance of a private downstream fork,
scoped and reviewed by its owner.

### Branches

One branch per hardware/OS floor, each starting from a *named upstream
commit* of a real, separately-licensed SDL2 fork (zlib, redistribution
permitted), with every fleet patch landed as a real commit on top instead of
being regenerated at build time by a port's build driver.

| Branch | Base | Upstream | SDL version | Status |
|---|---|---|---|---|
| `retro/panther-ppc-v2` | `matthewdeaves/panther-sdl2@oldmac` (3c721fce79) | halflife's own fork, itself `alex-free/panther-sdl2@bd33187` + 7 real commits | 2.0.3 | **canonical**, per manager decision 2026-09-13 15:12. SDL#1 fix landed on top, tagged `retro/panther-ppc-sdl1-fix-v2` |
| `retro/panther-ppc` | `alex-free/panther-sdl2@bd33187` | <https://github.com/alex-free/panther-sdl2> | 2.0.3 | **superseded, do not use.** Built from old-mac-build-host's 2026-07-27 snapshot, which predates halflife's real production history (`matthewdeaves/panther-sdl2`) by one joystick-backend commit (2026-08-21) — this branch ships without PowerPC gamepad support. Kept only because a force-push to remove it needs a human's go-ahead; see `retro/panther-ppc-v2` instead. Tag `retro/panther-ppc-sdl1-fix` is superseded the same way, by `retro/panther-ppc-sdl1-fix-v2`. |
| `retro/leopard-ppc` | `alex-free/leopard-sdl2@01e350c` | <https://github.com/alex-free/leopard-sdl2> | 2.0.6 | fleet hand-edit landed, tagged `retro/leopard-ppc-base`; in no shipped slice since old-mac-halflife v1.4.0 |
| `retro/x86_64-10.6` | upstream `release-2.0.22` (53dea9830), unmodified | <https://github.com/libsdl-org/SDL> | 2.0.22 | **canonical for the x86_64 floor.** No source patch: SDL#2 is a deployment-target artefact, fixed by building at `-mmacosx-version-min=10.6` instead of 10.7. Tagged `retro/x86_64-10.6-base`. Supersedes the placeholder name `retro/x86_64-10.5` from this repo's original floor list — the real, measured floor is 10.6 (Snow Leopard/mini-sl), not 10.5. |
| `retro/arm64` | not yet established | — | — | not started |

### x86_64 floor: SDL#2 finding

The shared `~/oldmac/sdl2-x86_64` dylib on mini-intel is stock SDL2 2.0.22
from the libsdl.org tarball, built by old-mac-halflife's `scripts/build-lion.sh`
at `-mmacosx-version-min=10.7` — not a legacy fork, no patches. The crash
(`_NSBackingPropertyOldScaleFactorKey` hard-linked, dyld failure on 10.6.8)
is purely because that symbol sits behind an Apple availability annotation:
below 10.7 it is automatically a **weak** import, at 10.7+ it's a **hard**
one. `~/oldmac/sdl2-snow-x86_64` (built at 10.6) already exists and already
ships correctly in Half-Life's 10.6 slice.

Weak-link audit recipe for this floor (verified 2026-09-13):

```sh
cat > probe.m <<'EOF'
#import <AppKit/AppKit.h>
NSString *probe(void) { return NSBackingPropertyOldScaleFactorKey; }
EOF
SDK=$(xcrun --sdk macosx --show-sdk-path)
clang -c -x objective-c -arch x86_64 -isysroot "$SDK" \
  -mmacosx-version-min=10.6 -fobjc-arc probe.m -o out.o
nm -m out.o | grep BackingPropertyOldScaleFactorKey
# pass: "(undefined) weak external _NSBackingPropertyOldScaleFactorKey"
# fail: "(undefined) external ..." with no "weak" — means the floor's SDK
# no longer marks this symbol weak below 10.7, or the deployment target
# regressed to 10.7+.
```

This uses the *current* SDK's availability annotations, not a real 10.6 SDK
— that's fine, since the annotation itself (`_NSBackingPropertyOldScaleFactorKey`
introduced 10.7) hasn't changed and Apple SDKs carry historical availability
metadata forward. A newer-symbol regression on this floor would fail the
same way a real 10.6 SDK link would.

`old-mac-half-life-1` still builds from `matthewdeaves/panther-sdl2` directly
(`scripts/build-pins.sh`, not this repo) as of the v1.9.18 RC. Repointing it
at `retro/panther-ppc-v2` is a follow-up (`old-mac-half-life-1` Triage
ticket, once filed) for after that RC, not before — the RC is already built
and installed fleet-wide from a byte-identical source.

### How each tree is built and verified

Provenance for the two PPC trees lives in
`old-mac-build-host/sources/sdl2-legacy/README.md` (peer repo, read-only from
here): git bundles of the upstream forks, a pinned commit per tree, and a
per-file sha256 manifest (`*.pin.sha256`) of a clean checkout at that pin.

To (re)verify a floor branch's base against its pin:

```sh
git clone /path/to/<tree>.bundle /tmp/check && cd /tmp/check
git checkout <pinned-commit>
shasum -a 256 -c /path/to/<tree>.pin.sha256   # every file must say OK
```

Both trees were re-verified this way before any commit was added:
panther 1018/1018 files, leopard 1157/1157 files.

Fleet patches are committed on top of the pin as their own commits, citing
which port script(s) they replace (e.g. old-mac-halflife's
`scripts/patch-panther-sdl-*.py`) so the history stays auditable. A bug fix
(e.g. SDL#1) is a further commit, verified against the actual file content
at that commit before being written, not against the reporting ticket's
claimed diff alone.

Per-floor weak-link audit: `scripts/weak-link-audit.sh <binary> <floor>`.
Run 2026-09-13 against alephone's real `sdl2-ppc-tiger103/lib/libSDL2.a` on
mini-intel2 (floor 10.3), now that buildhost#81's toolchain exists there.
Two findings, both load-bearing for how to read this script's output on a
PPC/gcc build:

- **Zero weak-import markers anywhere in the archive** (0 of 947 undefined
  symbols, archive-wide). The clang-only "weak vs hard" signal this script
  uses to catch SDL#2's class of bug does not discriminate anything on the
  old gcc-4.0/PowerPC toolchain — it may simply never mark anything weak.
  A "0 weak, N hard" result from this script on a PPC/gcc artifact is not
  itself a pass or fail; see the script's own LIMITS comment.
- **The real floor guarantee for this toolchain is structural, not a link-
  time property of the artifact**: both `build-ppc-panther.sh` and
  `build-ppc-tiger.sh` (old-mac-halflife) compile with `-isysroot
  /Developer/SDKs/MacOSX10.3.9.sdk -mmacosx-version-min=10.3` — the exact
  floor SDK, so there is no way to accidentally pick up a symbol newer than
  10.3, unlike SDL#2 where a modern/mismatched target let a 10.7-only
  symbol in. Confirmed by reading both build scripts directly, not just
  inferred from the linked artifact.

After resolving intra-archive references (most "undefined" symbols in one
`.o` are actually defined by another `.o` in the same archive and are not a
real floor dependency), the archive has 191 genuinely external symbols —
libSystem/CoreFoundation/CoreGraphics/Carbon/IOKit/pthread/objc-runtime
calls consistent with a 10.3-era SDL2 build.

Cross-checked all 191 against the real `/Developer/SDKs/MacOSX10.3.9.sdk`'s
own headers (`weak-link-audit.sh`'s optional 3rd argument): 185 found with
no newer-than-floor `AVAILABLE_MAC_OS_X_VERSION_10_N_AND_LATER` annotation,
0 flagged as a version risk, 0 using `WEAK_IMPORT_ATTRIBUTE`, and 6 "not
found in SDK headers" — all benign compiler/ABI internals, not OS APIs at
all: `___CFConstantStringClassReference` (the ObjC string-literal class
ref), `___udivdi3`/`___umoddi3` (libgcc 64-bit division helpers), and
`restGPR`/`restGPRx`/`saveGPR` (PowerPC prologue/epilogue helpers from
libgcc). None represent a floor risk. Full result posted to
matthewdeaves/SDL#1.

The first attempt at this cross-check silently died 68 symbols in (a
precompiled-header cache file broke grep; fixed in `weak-link-audit.sh`,
see its own history) — worth remembering that a script exiting 0 is not
proof it ran to completion; the wrapper's own trailing cleanup command
masked the real failure. Re-run and verified complete (191/191 accounted
for) before trusting this result.

Does **not** catch SDL#1/alephone#37's actual failure mode (an
`objc_msgSend` to a selector the runtime doesn't implement) — that's
dynamic dispatch, never a linked symbol, so no static `nm`/`otool` check on
a linked binary can see it.

### Boundaries

Buildhost owns `install-sdl2-trees.sh`, machines and pickers; this repo owns
source fixes only. The handoff to buildhost/a port is always a branch, a
tag, and a commit hash by mail — never direct access to another repo or
host from here.

