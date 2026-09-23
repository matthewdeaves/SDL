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
| ~~`retro/panther-ppc`~~ | `alex-free/panther-sdl2@bd33187` | <https://github.com/alex-free/panther-sdl2> | 2.0.3 | **deleted 2026-09-23** (user's go-ahead). It was superseded by `retro/panther-ppc-v2`: built from buildhost's 2026-07-27 snapshot, it had no PowerPC gamepad support. It was branch `7acc64195`, with tag `retro/panther-ppc-sdl1-fix` (tag object `d9bbfc60`). No backup is kept (fix-forward rule, 2026-09-23). |
| `retro/leopard-ppc` | `alex-free/leopard-sdl2@01e350c` | <https://github.com/alex-free/leopard-sdl2> | 2.0.6 | fleet hand-edit landed, tagged `retro/leopard-ppc-base`; in no shipped slice since old-mac-halflife v1.4.0 |
| `retro/x86_64-10.6` | upstream `release-2.0.22` (53dea9830), unmodified | <https://github.com/libsdl-org/SDL> | 2.0.22 | **canonical for the x86_64 floor.** No source patch: SDL#2 is a deployment-target artefact, fixed by building at `-mmacosx-version-min=10.6` instead of 10.7. Tagged `retro/x86_64-10.6-base`. Supersedes the placeholder name `retro/x86_64-10.5` from this repo's original floor list — the real, measured floor is 10.6 (Snow Leopard/mini-sl), not 10.5. |
| `retro/tiger-i386` | `retro/panther-ppc-v2` (1b299830c) + 2 commits | halflife's panther-sdl2 fork, as above | 2.0.3 | **canonical for the i386 floor (10.4).** Adds `f26d2d463` (old display-mode API on every OS below a 10.6 floor) and `4551b9a95` (no Spaces below a 10.7 floor); both are no-ops on ppc. Tagged `retro/tiger-i386-base` (SDL#7). |
| `retro/arm64` | upstream `release-2.32.4` (2359383fc), unmodified | <https://github.com/libsdl-org/SDL> | 2.32.4 | **canonical for the arm64 floor (macOS 11.0).** Equals the signed `SDL2-2.32.4.tar.gz` (sha256 `f15b4782…f934`) that halflife/quake2/quake3 `build-arm64.sh` fetch. Tagged `retro/arm64-base` (SDL#6). |

SDL 1.2 lives in the sibling fork **matthewdeaves/SDL-1.2** (checkout
`../SDL-1.2`; issues stay here). `retro/panther-ppc` is the ppc/10.3 floor:
tag `retro/panther-ppc-base` rebuilds today's shipped slice byte-identically
(SDL#4), and `retro/panther-ppc-sdl5-fix` adds the 16-bpp desktop fix and is
released with its tested slice (SDL#5). That repo's CLAUDE.md has the details.

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

### i386 floor: SDL#7

SDL2 2.0.22 (halflife's i386 prefix) `#error`s below 10.6. The 10.4 answer is
the panther fork's 2.0.3, built for i386 with Apple clang against the real
`MacOSX10.4u.sdk` (on imac-2019), so no GCC with Objective-C is needed for SDL
itself:

```sh
FLAGS="-arch i386 -mmacosx-version-min=10.4 -isysroot ~/SDKs/MacOSX10.4u.sdk"
../src/configure --host=i386-apple-darwin8 --disable-shared --enable-static \
  --without-x --disable-haptic CC="clang $FLAGS" CFLAGS="-O2 $FLAGS" LDFLAGS="$FLAGS"
```

- It has to be an out-of-tree build: the fork commits a ppc-generated
  `include/SDL_config.h`, and only an out-of-tree build puts its own first.
- `--disable-haptic`: the pre-10.5 joystick backend has no `ffservice`, so
  `haptic/darwin` doesn't compile. The ppc builds disable haptic too.
- Measured 2026-09-23. The audit says declared 10.4, 0 weak, and 213 hard
  externals: 211 are clean against the 10.4u headers, and the other 2 are
  compiler internals. An SDL_Init + GL window probe runs on mini-sl (10.6.8,
  Core 2 Duo, GeForce 9400), including a real 640x480 mode switch, desktop
  fullscreen, and a restore, 3/3 runs. Before `f26d2d463`, the same probe
  failed there with "The video driver did not add any displays".
- Not run on 10.4 or 10.5 Intel: the fleet has no such host (alephone#31).
  The display path that runs there is the same code the ppc builds run on
  real 10.3-10.5.

### arm64 floor: SDL#6

The three arm64 build drivers (old-mac-halflife, old-mac-quake2 and
old-mac-quake3 `scripts/build-arm64.sh`) each fetch
`https://www.libsdl.org/release/SDL2-2.32.4.tar.gz` and build it at
`-mmacosx-version-min=11.0`, with no source patch. `retro/arm64` is that
release tag, so the tarball is the source of record. Verified 2026-09-23:

- tarball sha256
  `f15b478253e1ff6dac62257ded225ff4e7d0c5230204ac3450f1144ee806f934`, the
  same from libsdl.org and from the GitHub release asset. It has a good GPG
  signature (`.sig`) from Sam Lantinga, key
  `1528635D8053A57F77D1E08630A59377A7763BE6`.
- its 1749 files equal `git archive release-2.32.4`, apart from the
  tarball's `.git-hash` (2359383fc…) and `REVISION.txt`, and the git tree's
  `.github`/`.gitignore`.
- the quake2 and quake3 cached source trees on the workstation
  (`~/.cache/oldmac-q{2,3}-arm64/src/SDL2-2.32.4`) match that tarball at
  1749/1749 files. halflife's `/tmp` tree is gone.
- `weak-link-audit.sh` on `~/oldmac/sdl2-arm64/lib/libSDL2-2.0.0.dylib`
  at floor 11.0: declared minos 11.0 and no mismatch. That needed
  `c2111589d`: the script used to read `LC_BUILD_VERSION`'s ld version
  (1267.0) as the floor. With minos at the floor, clang weak-imports any
  symbol newer than 11.0 (45 weak here).

Built dylibs are not byte-identical between ports: each one embeds its own
prefix, and quake2/quake3 add `-O2`. The pin is on the source.

### RC release gate (manager, 2026-09-23)

deps checks every bundled SDL slice of each port RC DMG and posts
PASS/FAIL per slice on the RC's ticket. A mismatch blocks promotion.
Per slice:
- **Source:** the embedded revision string. 2.0.3 fork: `hg-8628:b558f99d48f0`.
  2.0.22: `libsdl-org/SDL.git@53dea9830`. 2.32.4:
  `SDL-release-2.32.4-0-g2359383fc`. Check it against the port's pin file at
  the RC commit.
- **Floor:** the declared floor against the port's README. ppc slices from
  gcc-4.0 declare none, so rely on the 10.3.9 SDK link.
- **Audit:** `weak-link-audit.sh` at that floor. Intel slices at 10.6 must
  import `_NSBackingPropertyOldScaleFactorKey` weak (SDL#2).
- **Fix markers:** an SDL2 ppc build must have no `drain` selector string
  (SDL#1). An SDL 1.2 ppc slice must be the SDL#5 build (sha256 `07cc046e…`,
  or the same load commands plus the port's install id).
- Carve ppc members out of fat files with the fat header; modern `lipo` can't
  thin ppc.

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
Since `c16deb5c3` it parses **linked** binaries correctly. Before that it
mis-parsed them (lazy-bound imports never matched, and `(from Lib)` was
taken as the symbol name), so a dylib or executable could read "0 hard"
and look like a pass. It now exits 3 when nm yields no external symbols.
Run it on the workstation: mini-intel's own `nm` can't parse newer
binaries.
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

