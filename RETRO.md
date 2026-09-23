# Retro branches (matthewdeaves/SDL)

This fork is the source record for the SDL2 in the old-mac ports (Mac OS X
10.3+, PowerPC, Intel, Apple Silicon). `main` follows upstream and isn't
built. Each floor has a `retro/*` branch from a named upstream commit, one
commit per fleet patch, and a tag for the tested state. Half-Life and Aleph
One build their PowerPC and i386 SDL from those tags, pinned by hash. The
x86_64 and arm64 tags record the upstream release the ports download from
libsdl.org; Quake II and III use SDL2 only on arm64, sha256-checked.

| Branch | Tag | SDL | Floor | Base |
|---|---|---|---|---|
| `retro/panther-ppc-v2` | `retro/panther-ppc-sdl1-fix-v2` | 2.0.3 | PowerPC, 10.3 | alex-free/panther-sdl2 + 10.3 API-floor fixes, pre-10.5 joystick backend, Panther `-drain` crash fix (#1) |
| `retro/tiger-i386` | `retro/tiger-i386-base` | 2.0.3 | i386, 10.4 | `retro/panther-ppc-v2` + two Cocoa fixes for 10.4-floor builds running on 10.6+ (#7) |
| `retro/x86_64-10.6` | `retro/x86_64-10.6-base` | 2.0.22 | x86_64, 10.6 | upstream `release-2.0.22`, unmodified; build it at `-mmacosx-version-min=10.6` (#2) |
| `retro/arm64` | `retro/arm64-base` | 2.32.4 | arm64, 11.0 | upstream `release-2.32.4`, unmodified; equals the signed `SDL2-2.32.4.tar.gz`, sha256 `f15b4782…f934` (#6) |
| `retro/leopard-ppc` | `retro/leopard-ppc-base` | 2.0.6 | PowerPC, 10.5 | alex-free/leopard-sdl2 + one fleet fix (`87ed277`); not in any current release |

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

Ticket numbers are this repo's issues. SDL is zlib-licensed (`LICENSE.txt`).
