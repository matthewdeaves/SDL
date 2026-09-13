#!/usr/bin/env python3
"""
selector-audit.py <source-root> <sdk-path>

Static selector-literal audit for Objective-C source against a floor SDK's
public headers. SDL#1 (alephone#37) was exactly this class of bug: a call
to [autorelease_pool drain], a real Cocoa method that simply does not exist
in Panther's NSAutoreleasePool.h. No static symbol/link check can see that
(it's dynamic dispatch, never a linked symbol -- see weak-link-audit.sh's
own LIMITS note) but a selector-vs-header check can, in principle, since the
selector's absence from the floor SDK's own header is exactly the fact that
matters.

What it does:
  1. Finds local variable declarations of a known Foundation/AppKit class
     (`NSFoo *var;` / `NSFoo* var =`).
  2. Finds Objective-C message sends -- to those variables, and to explicit
     class names via `[NSFoo ...]` / `[[NSFoo alloc] ...]`.
  3. For each resolved (class, selector) pair, checks whether the SDK's own
     header for that class declares a method with that selector. Falls back
     to NSObject.h as a coarse base-class check if the class's own header
     doesn't declare it.
  4. Flags any selector not found in either as NOT DECLARED -- worth a
     direct look, the same way alephone found SDL#1 by hand.

SCOPE AND KNOWN LIMITS -- read before trusting a clean result:
  - Regex-based, not a real Objective-C parser. A multi-line message send, a
    selector built via a macro or ivar access through a non-obvious cast,
    or a message sent through an `id`/protocol-typed variable with no
    resolvable class, is silently skipped. This is a best-effort net, not a
    completeness proof, and a clean run does NOT mean there is no bug like
    SDL#1 left -- only that this pass didn't find one in what it could see.
  - Only the receiver class's own header is checked, plus NSObject as a
    single coarse fallback -- it does NOT walk the real superclass chain.
    A selector genuinely inherited from an intermediate class (e.g.
    NSResponder methods on an NSView subclass) will show as a false
    "NOT DECLARED". Confirm every hit by hand against the actual class
    hierarchy before treating it as a real bug.
  - Limited to a fixed list of Foundation/AppKit classes worth tracking
    (see CLASSES below) -- add to that list for a wider net.
  - Only matches `- (type)sel;` / `+ (type)sel;` declarations, not
    `@property (class, ...) Type *sel;` -- irrelevant for a pre-10.5 floor
    SDK (ObjC properties didn't exist before Leopard, class properties much
    later), but will under-check a modern SDK. Confirmed 2026-09-13 running
    this against a modern SDK for validation: every remaining false
    positive after fixing the NSObject-header bug (see git history) was a
    class-property declaration like NSFileManager's `defaultManager`.
"""
import re
import sys
import pathlib

CLASSES = [
    "NSAutoreleasePool", "NSWindow", "NSView", "NSApplication", "NSApp",
    "NSCursor", "NSEvent", "NSScreen", "NSOpenGLContext", "NSOpenGLView",
    "NSOpenGLPixelFormat", "NSString", "NSMutableString", "NSArray",
    "NSMutableArray", "NSDictionary", "NSMutableDictionary", "NSNumber",
    "NSNotificationCenter", "NSPasteboard", "NSImage", "NSBitmapImageRep",
    "NSColor", "NSMenu", "NSMenuItem", "NSAlert", "NSData", "NSURL",
    "NSFileManager", "NSProcessInfo", "NSBundle", "NSTrackingArea",
]

DECL_RE = re.compile(
    r"\b(" + "|".join(CLASSES) + r")\s*\*\s*([A-Za-z_]\w*)\b"
)
# Unary message send: [recv selector]
UNARY_RE = re.compile(r"\[\s*([A-Za-z_]\w*)\s+([A-Za-z_]\w*)\s*\]")
# Keyword message send: [recv key1:...key2:...]
KEYWORD_RE = re.compile(
    r"\[\s*([A-Za-z_]\w*)\s+((?:[A-Za-z_]\w*:\s*)+)"
)


def find_class_headers(sdk_path: pathlib.Path, klass: str) -> list:
    # More than one file can be named e.g. NSObject.h -- Foundation's own
    # NSObject.h is a thin wrapper declaring NSCopying/NSCoding etc, while
    # the actual class interface (+alloc, -init, ...) lives in
    # usr/include/objc/NSObject.h. Taking "the first match" silently missed
    # every NSObject method on a real SDK during development of this
    # script. Union every match instead of picking one.
    return list(sdk_path.glob(f"**/{klass}.h"))


def declared_selectors(headers: list) -> set:
    sels = set()
    for header in headers:
        try:
            text = header.read_text(errors="replace")
        except OSError:
            continue
        # unary/keyword method declarations: - (type)name; or - (type)k1:(t)a k2:(t)b;
        # Method attributes/availability macros can follow on the same
        # logical line before the terminating ';' -- not matched here, only
        # the selector itself needs to be, so that's fine.
        for m in re.finditer(r"^[-+]\s*\([^)]*\)\s*((?:[A-Za-z_]\w*:)+|[A-Za-z_]\w*)", text, re.M):
            sels.add(m.group(1))
    return sels


def audit(source_root: pathlib.Path, sdk_path: pathlib.Path):
    header_cache = {}
    findings = []
    checked = 0

    for path in sorted(source_root.glob("src/**/*.m")):
        text = path.read_text(errors="replace")
        var_class = dict(DECL_RE.findall(text))

        sends = []
        for m in UNARY_RE.finditer(text):
            recv, sel = m.group(1), m.group(2)
            sends.append((recv, sel, m.start()))
        for m in KEYWORD_RE.finditer(text):
            recv, keys = m.group(1), m.group(2)
            sel = "".join(k.strip() + ":" for k in keys.split(":") if k.strip())
            sends.append((recv, sel, m.start()))

        for recv, sel, pos in sends:
            klass = recv if recv in CLASSES else var_class.get(recv)
            if not klass:
                continue
            if klass not in header_cache:
                hdrs = find_class_headers(sdk_path, klass)
                header_cache[klass] = (hdrs, declared_selectors(hdrs))
            hdrs, sels = header_cache[klass]
            if not hdrs:
                continue  # class not in this SDK at all -- separate, bigger problem
            checked += 1
            if sel not in sels:
                if "NSObject" not in header_cache:
                    nsobject_hdrs = find_class_headers(sdk_path, "NSObject")
                    header_cache["NSObject"] = (nsobject_hdrs, declared_selectors(nsobject_hdrs))
                _, nsobject_sels = header_cache["NSObject"]
                if sel in nsobject_sels:
                    continue
                line = text.count("\n", 0, pos) + 1
                findings.append((str(path.relative_to(source_root)), line, klass, sel,
                                  ", ".join(str(h) for h in hdrs)))

    return findings, checked


def main():
    if len(sys.argv) != 3:
        print(f"usage: {sys.argv[0]} <source-root> <sdk-path>", file=sys.stderr)
        return 2
    source_root = pathlib.Path(sys.argv[1])
    sdk_path = pathlib.Path(sys.argv[2])
    if not source_root.is_dir():
        print(f"!! not a directory: {source_root}", file=sys.stderr)
        return 2
    if not sdk_path.is_dir():
        print(f"!! not a directory: {sdk_path}", file=sys.stderr)
        return 2

    findings, checked = audit(source_root, sdk_path)
    print(f"== selector audit: {source_root} vs {sdk_path} ==")
    print(f"(class, selector) pairs resolved and checked: {checked}")
    print()
    if not findings:
        print("no NOT DECLARED selectors found in what this pass could resolve")
        print("(read the LIMITS in this script's docstring before treating that as proof)")
        return 0
    print(f"-- {len(findings)} selector(s) not declared in the floor SDK's header (or NSObject) --")
    for f, line, klass, sel, hdr in findings:
        print(f"  {f}:{line}: [{klass} {sel}]  -- not in {hdr}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
