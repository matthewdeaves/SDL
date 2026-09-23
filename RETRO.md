# Retro branches (matthewdeaves/SDL)

This fork keeps the SDL2 builds used by the old-mac ports of Half-Life,
Quake II, Quake III, QuakeSpasm and Aleph One. They run on Mac OS X 10.3 and
later, on PowerPC, Intel and Apple Silicon. `main` follows upstream and isn't
used for builds. Each hardware/OS floor has its own `retro/*` branch. Every
branch starts from a named upstream commit, with each fleet patch as its own
commit, and a tag marks the tested state. Ports build from a tag and pin its
commit hash.

| Branch | Tag | SDL | Floor | Base |
|---|---|---|---|---|
| `retro/panther-ppc-v2` | `retro/panther-ppc-sdl1-fix-v2` | 2.0.3 | PowerPC, 10.3 | alex-free/panther-sdl2 + 10.3 API-floor fixes, pre-10.5 joystick backend, Panther `-drain` crash fix (#1) |
| `retro/tiger-i386` | `retro/tiger-i386-base` | 2.0.3 | i386, 10.4 | `retro/panther-ppc-v2` + two Cocoa fixes for 10.4-floor builds running on 10.6+ (#7) |
| `retro/x86_64-10.6` | `retro/x86_64-10.6-base` | 2.0.22 | x86_64, 10.6 | upstream `release-2.0.22`, unmodified; build it at `-mmacosx-version-min=10.6` (#2) |
| `retro/arm64` | `retro/arm64-base` | 2.32.4 | arm64, 11.0 | upstream `release-2.32.4`, unmodified; equals the signed `SDL2-2.32.4.tar.gz`, sha256 `f15b4782…f934` (#6) |
| `retro/leopard-ppc` | `retro/leopard-ppc-base` | 2.0.6 | PowerPC, 10.5 | alex-free/leopard-sdl2 + fleet edits; not in any current release |

Build notes:

- The 2.0.3 trees commit a ppc-generated `include/SDL_config.h`, so always
  configure out of tree. Pass `--disable-haptic`, because the pre-10.5
  joystick backend has no force-feedback support.
- i386 builds with Apple clang against the real `MacOSX10.4u.sdk`:
  `-arch i386 -mmacosx-version-min=10.4 -isysroot <10.4u SDK>`.
- `scripts/weak-link-audit.sh <binary> <floor> [<floor SDK>]` (on `main`)
  lists the external symbols a build needs, checks its declared minimum OS,
  and, if given the floor SDK, cross-checks the symbols against that SDK's
  headers.

The ticket numbers above are this repo's issues, which hold the evidence for
each floor. SDL is zlib-licensed (`LICENSE.txt`). Nothing here is sent
upstream.
