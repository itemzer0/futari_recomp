# Vendored ReXGlue SDK

This directory is a **vendored copy** of the ReXGlue SDK, not a submodule. It was
taken verbatim from `springah/rexglue-sdk` (a fork of
[rexglue/rexglue-sdk](https://github.com/rexglue/rexglue-sdk)) at tag
**v0.9.3.1**, commit `040d5bca3de2f2970e9e29b4bf789739a48f5301`.

This port maintains its own SDK from here: fixes land in this tree directly and
are cherry-picked to/from the fork or upstream when they apply. There is no pin
to bump — the tree *is* the SDK.

## Deviations from the v0.9.3.1 tag

- **`tools/binutils/` is dropped** (~130 MB of prebuilt Windows PowerPC binutils
  plus `cygwin1.dll`). It is used only by `cmake/ppc_test_pipeline.cmake` to
  assemble the SDK's own PPC test cases, which are not run from this repository —
  and redistributing prebuilt GPL binaries in a public source repo would carry a
  corresponding-source obligation. Restore it from the fork tag if the test
  pipeline is ever needed here.
- **`VERSION` added** (contains the vendored tag). `cmake/rex_version.cmake` is
  patched to read it before falling back to `git describe`: a vendored tree has
  no git history of its own, and `git describe` here would otherwise pick up the
  *host* repository's tags (which hard-fails the version floor check).
- **Nested third-party dependencies stay git submodules**, now registered in the
  host repository's root `.gitmodules` at `thirdparty/rexglue-sdk/thirdparty/*`,
  pointing at their public upstreams. The SDK's own `.gitmodules` file is kept
  in place for reference but git does not read it (only the root one counts).

## Auditing against the source tag

```sh
git -C <a-rexglue-sdk-checkout> archive v0.9.3.1 | tar -x -C /tmp/sdk-tag
diff -r --exclude=tools /tmp/sdk-tag thirdparty/rexglue-sdk
# expected deltas: VERSION, VENDORED.md, cmake/rex_version.cmake, empty submodule dirs
```
