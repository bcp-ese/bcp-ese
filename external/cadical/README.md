# CaDiCaL dependency

CaDiCaL is the only SAT backend used by this source tree.

The primary experiments use CaDiCaL `rel-1.9.5` at commit
`146207318796f094dcded87349a64f0c6927309e`.

The checked-in header has SHA-256
`c5c2068ce767e4b87915f657f1963a816b28bdf958346efcb81f9ffe1ff3b679`, which matches
`src/cadical.hpp` at that tag. The checked-in static archive is platform-specific and is
not linkable with AppleClang on macOS/arm64. Build the exact dependency locally instead
of replacing the tracked archive with a binary from another CaDiCaL version.

One reproducible build procedure is:

```sh
git clone https://github.com/arminbiere/cadical.git cadical-1.9.5
git -C cadical-1.9.5 checkout 146207318796f094dcded87349a64f0c6927309e
(cd cadical-1.9.5 && ./configure && make -j4)

CADICAL_LIB="$PWD/cadical-1.9.5/build/libcadical.a" \
CADICAL_INCLUDE_DIR="$PWD/cadical-1.9.5/src" \
./build.sh
```

For a reported experiment, record the CaDiCaL commit, compiler version, build command,
and SHA-256 of both `libcadical.a` and the resulting `bcp` executable.
