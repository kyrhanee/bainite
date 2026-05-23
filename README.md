<div align="center">

```
 __          _       _ __
    / /_  ____ _(_)___  (_) /____
    / __ \/ __ `/ / __ \/ / __/ _ \
   / /_/ / /_/ / / / / / / /_/  __/
  /_.___/\__,_/_/_/ /_/_/\__/\___/
```

### a one-way executable packer for Windows PE (x86_64)

[![License](https://img.shields.io/badge/license-MIT_%2F_Apache--2.0-blue?style=flat-square)](#license)
[![Rust](https://img.shields.io/badge/rust-stable-orange?style=flat-square&logo=rust)](https://www.rust-lang.org)
[![Platform](https://img.shields.io/badge/platform-windows--x86__64-lightgrey?style=flat-square)](#supported-targets)

</div>

---

## what

bainite shrinks Windows PE (x86_64) executables further than UPX on real-world
binaries. LZMA2 at extreme settings, BCJ x86 filter, and an in-memory runtime
stub.

**bainite is one-way.** It compresses; it does not extract. There is no
`unpack` command, no `unpack` endpoint, no FFI to recover the original bytes.
Once a binary is packed, only the runtime stub can decompress it and execute
it.

## install

Pre-built binaries: see **[releases](../../releases/latest)**.

| binary                              | what                              |
| ----------------------------------- | --------------------------------- |
| `bainite.exe`                       | command-line packer               |
| `bainited.exe`                      | REST + gRPC daemon                |
| `bainite_native.dll` + `bainite.h`  | C ABI for embedding               |

## quick start

```sh
bainite app.exe                    # writes app.bainite.exe (ultra preset)
bainite -i app.bainite.exe         # show metadata
bainite -t app.bainite.exe         # exit 0 if packed, 1 otherwise
```

## cli

```
bainite [OPTIONS] FILE

  -o, --output PATH          output file (default: FILE.bainite.exe)
  -l, --level LEVEL          fast | balanced | max | ultra   (default: ultra)
  -f, --filter FILTER        none | bcj                       (default: bcj)
      --keep-overlay         keep PE overlay bytes (default: strip)
  -i, --info                 show metadata of a packed binary
  -t, --test                 exit 0 if packed, 1 otherwise
  -V, --version              show version
  -h, --help                 show help
```

Defaults produce the strongest compression. `LZMA2` is the only payload
algorithm; `bcj` is the x86 branch-call filter that boosts compressibility of
machine code.

## supported targets

| Target              | Pack | Stub      | Status |
| ------------------- | :--: | --------- | ------ |
| Windows PE x86_64   | yes  | in-memory | active |

ARM64 / i686 are not supported.

## daemon

`bainited` exposes the engine over the network for CI or any language that
does not link the FFI.

```sh
bainited --bind 127.0.0.1:7575 --grpc-bind 127.0.0.1:7576 --token SECRET
```

REST:

| Method | Path             | Body                          | Returns           |
| ------ | ---------------- | ----------------------------- | ----------------- |
| `GET`  | `/healthz`       | —                             | `{status,version}` |
| `POST` | `/v1/pack`       | multipart `file`, `options`   | packed binary     |
| `POST` | `/v1/info`       | multipart `file`              | JSON metadata     |

`options` is JSON: `{"filter": "none"|"bcj", "level": <int>}` — both fields
optional.

gRPC: service `bainite.v1.Packer` — see
[`crates/bainite-proto/proto/bainite.proto`](crates/bainite-proto/proto/bainite.proto).

## embedding (C ABI)

```c
#include "bainite.h"

BainiteOptions opts = bainite_default_options();
BainiteReport  rep  = {0};

int rc = bainite_pack_file("app.exe", "app.bainite.exe", &opts, &rep);
if (rc != BAINITE_OK) {
    char buf[256];
    bainite_last_error(buf, sizeof buf);
    fprintf(stderr, "pack failed: %s\n", buf);
}
```

Works from Go (cgo), Python (ctypes), C#/.NET, Java (JNA), Node.js (ffi-napi).

## architecture

```
crates/
├── bainite-format    container schema (header + manifest, no_std)
├── bainite-core      PE parsing, compression, BCJ, packer, inspector
├── bainite-stub      no_std runtime — embedded into every packed binary
├── bainite-cli       the `bainite` binary
├── bainite-server    the `bainited` daemon — REST + gRPC
├── bainite-ffi       C ABI for embedding from any language
└── bainite-proto     gRPC schema and generated bindings
```

## how it works

1. parse the PE — sections, imports, relocations, entry point
2. apply the BCJ x86 filter — turn relative `call`/`jmp` into absolute targets
3. compress with LZMA2 at preset 9 + extreme (nice_len 273, depth 999)
4. emit a 192-byte header + manifest + payload
5. graft the result into a fresh `.bainite` section of a stub PE

at runtime the stub locates `.bainite`, decompresses LZMA2, reverses BCJ,
writes the recovered binary to a temp file, and runs it.

## build

```
cargo build --workspace --release
```

The stub is built separately (it's `no_std`) and embedded by `bainite-core`:

```
cd crates/bainite-stub
cargo build --release --target x86_64-pc-windows-msvc
copy target\x86_64-pc-windows-msvc\release\bainite-stub.exe ^
     ..\bainite-core\embedded\stub-pe-x86_64.bin
```

After that, rebuild `bainite-core` so `include_bytes!` picks up the new stub.

Toolchain notes for fresh installs: see [`DEVELOPING.md`](DEVELOPING.md).

## roadmap

- ARM64 PE
- Per-section compression with per-section filters
- Polymorphic stub naming
- Optional anti-debug

## license

dual-licensed under [MIT](LICENSE-MIT) or [Apache-2.0](LICENSE-APACHE).
