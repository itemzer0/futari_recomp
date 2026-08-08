# Notice — what the licence covers

This file states the scope of [LICENSE](LICENSE). It is a clarification, not a
grant, and adds no conditions to the BSD 3-Clause licence itself.

## Covered

The contents of this repository: the host application (`src/`), the tooling
(`scripts/`, `extract_assets.bat`, `mac-build/`), the codegen inputs (`*.toml`),
the build files, and the documentation.

## Not covered

The licence does NOT cover, and this project does not grant any rights to:

  * **Mushihimesama Futari** — its code, data, audio, or artwork. All rights are
    owned by Cave Interactive Co., Ltd. No game content is distributed here.
  * **The recompiled C++ under `generated/`.** That is a machine translation of
    the game's own executable code, produced on your machine from a copy you
    own. It is a derivative of Cave's copyrighted work, not of this repository,
    and it is neither distributed here nor licensed by this project.
  * **`screenshots/title.png`** — a screenshot of the game, included for
    identification purposes only.

## Third-party components

The ReXGlue SDK (`thirdparty/rexglue-sdk`) is a separate project under its own
BSD 3-Clause licence, and is itself derived from the Xenia project
(Copyright (c) 2022, Ben Vanik and Xenia project contributors).
