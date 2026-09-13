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
| `retro/panther-ppc` | `alex-free/panther-sdl2@bd33187` | <https://github.com/alex-free/panther-sdl2> | 2.0.3 | fleet patches + SDL#1 fix landed, tagged `retro/panther-ppc-sdl1-fix` |
| `retro/leopard-ppc` | `alex-free/leopard-sdl2@01e350c` | <https://github.com/alex-free/leopard-sdl2> | 2.0.6 | fleet hand-edit landed, tagged `retro/leopard-ppc-base`; in no shipped slice since old-mac-halflife v1.4.0 |
| `retro/x86_64-10.5` | not yet established | — | — | source tree at `~/oldmac/sdl2-x86_64` (mini-intel) has no recorded provenance; identifying it is the open prerequisite (SDL#2) |
| `retro/arm64` | not yet established | — | — | not started |

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

Per-floor weak-link audit (`nm -m` undefined externals against that floor's
SDK, must fail on a hard reference to a symbol newer than the floor) is not
yet implemented for the PPC floors — blocked on an ObjC-capable PPC
toolchain (buildhost#81). Do not claim a floor's ceiling is enforced until
that audit exists and passes.

### Boundaries

Buildhost owns `install-sdl2-trees.sh`, machines and pickers; this repo owns
source fixes only. The handoff to buildhost/a port is always a branch, a
tag, and a commit hash by mail — never direct access to another repo or
host from here.

