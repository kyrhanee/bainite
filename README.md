```
    __          _       _  __
   / /_  ____ _(_)___  (_) /____
  / __ \/ __ `/ / __ \/ / __/ _ \
 / /_/ / /_/ / / / / / / /_/  __/
/_.___/\__,_/_/_/ /_/_/\__/\___/
```

A modern executable packer for Windows PE. Compresses your `.exe` with LZMA2
at extreme settings and a BCJ x86 filter, then ships it as a single self-running
binary with a 16 KiB runtime.

## what you get

- **Heavy compression.** LZMA2 preset 9 with `nice_len=273` and `depth=999`,
  combined with a BCJ x86 preprocessor that converts relative `call`/`jmp`
  offsets to absolute targets so the entropy coder finds longer matches.
- **In-memory runtime.** The packed binary maps the original image directly
  into the current process — sections, relocations, imports, page protections,
  TLS callbacks, jump to OEP. No temp files, no `CreateProcess`, no extra disk
  I/O after the first read.
- **Tiny stub.** 16 KiB. On binaries of 5 MiB and up the overhead is invisible.
- **Adaptive strategy.** `--algo auto` benchmarks Zstd and LZMA2 for the input
  and picks whichever produces the smaller payload.
- **Optional ChaCha20** over the compressed payload for opaque distribution.
- **Three integration modes.** Command-line tool, REST + gRPC daemon, C ABI
  shared library — embed from any language with FFI.
- **Wide PE support.** Handles standard executables and native-subsystem
  images (drivers, kernel components).

## results

```
bainite.exe       1.38 MiB  →    510 KiB     (-64%)
bainited.exe      3.40 MiB  →   1.20 MiB     (-65%)
real-world app    3.60 MiB  →   1.26 MiB     (-65%)
large native exe 25.10 MiB  →   5.43 MiB     (-78%)
```

`bainite.exe` and `bainited.exe` in this release are themselves packed by
bainite.

## download

Latest binaries: **[Releases](../../releases/latest)**.

| file                  | size     | what                          |
| --------------------- | -------: | ----------------------------- |
| `bainite.exe`         | 510 KiB  | command-line packer           |
| `bainited.exe`        | 1.20 MiB | REST + gRPC daemon            |
| `bainite_native.dll`  | 760 KiB  | C ABI shared library          |
| `bainite.h`           |   2 KiB  | C header                      |

Drop `bainite.exe` somewhere on `PATH`.

## quick start

```sh
bainite pack  app.exe              # writes app.bainite.exe
bainite info  app.bainite.exe      # shows metadata
bainite bench app.exe              # ratio table for the available algorithms
```

Defaults: `--level ultra --algo lzma2 --filter bcj`.

## cli

```
bainite pack  <input> [-o <output>]
                      [--level <fast|balanced|max|ultra>]
                      [--algo <lzma2|zstd|stored|auto>]
                      [--filter <none|bcj|bcj2>]
                      [--encrypt]
                      [--stub <path>]

bainite info  <input> [--json]
bainite bench <input> [--level <N>]
```

| option       | default | description                                          |
| ------------ | :-----: | ---------------------------------------------------- |
| `--level`    | `ultra` | preset for the chosen algorithm                      |
| `--algo`     | `lzma2` | `auto` benchmarks Zstd and LZMA2 and picks the best  |
| `--filter`   | `bcj`   | x86 preprocessor: `bcj` (classic) or `bcj2` (split)  |
| `--encrypt`  | off     | apply ChaCha20 over the compressed payload           |
| `--stub`     | embedded| path to a custom stub PE                             |

### examples

```sh
bainite pack app.exe                                 # default lzma2 + bcj + ultra
bainite pack app.exe --level fast                    # quick CI pass
bainite pack app.exe --algo auto --filter bcj2       # let the engine pick
bainite pack app.exe --encrypt                       # opaque payload
bainite info app.bainite.exe --json                  # machine-readable
bainite bench big.exe --level 22                     # head-to-head
```

## daemon

Run the engine over the network — useful for CI, packaging pipelines, or
languages that don't link the FFI.

```sh
bainited --bind 127.0.0.1:7575 --grpc-bind 127.0.0.1:7576 --token SECRET
```

`bainited` binds to `127.0.0.1` by default and refuses non-loopback hosts
without `--token`.

REST endpoints:

| Method | Path        | Body                          | Returns           |
| ------ | ----------- | ----------------------------- | ----------------- |
| `GET`  | `/healthz`  | —                             | `{status,version}`|
| `POST` | `/v1/pack`  | multipart `file`, `options`   | packed binary     |
| `POST` | `/v1/info`  | multipart `file`              | JSON metadata     |

```sh
curl -F file=@app.exe \
     -F 'options={"algorithm":"lzma2","level":22,"filter":"bcj"}' \
     -o app.bainite.exe \
     http://localhost:7575/v1/pack
```

gRPC: service `bainite.v1.Packer` with three RPCs — `Pack`, `Info`, `Health`.

## C ABI

```c
#include "bainite.h"

BainiteOptions opts = bainite_default_options();
BainiteReport  rep  = {0};

if (bainite_pack_file("app.exe", "app.bainite.exe", &opts, &rep) != BAINITE_OK) {
    char err[256];
    bainite_last_error(err, sizeof err);
    fprintf(stderr, "pack failed: %s\n", err);
}
```

| function                      | purpose                                       |
| ----------------------------- | --------------------------------------------- |
| `bainite_version()`           | version string                                |
| `bainite_default_options()`   | sane defaults (LZMA2, BCJ, ultra)             |
| `bainite_pack_file()`         | pack from path to path                        |
| `bainite_pack_buffer()`       | pack from memory to memory                    |
| `bainite_is_packed()`         | non-zero if the file is a bainite container   |
| `bainite_inspect()`           | read metadata into `BainiteReport`            |
| `bainite_last_error()`        | retrieve the last error message               |

Works from Go (cgo), Python (ctypes), C#/.NET, Java (JNA), Node.js (ffi-napi).

## how it works

```
1.  parse PE                  →  sections, imports, relocs, OEP, image base
2.  apply BCJ x86             →  relative call/jmp → absolute targets
3.  compress with LZMA2 e9    →  preset 9 + extreme (nice_len 273, depth 999)
4.  optional ChaCha20         →  scramble the payload
5.  emit container            →  192-byte header + manifest + payload
6.  graft into stub PE        →  new ".bainite" section of the stub
```

At launch the embedded stub locates `.bainite`, restores the image in memory
and jumps to the original entry point. Disk is untouched after the first
read.

## supported targets

| target                | status   |
| --------------------- | -------- |
| Windows PE x86_64     | active   |
| Windows PE i686       | active   |

## roadmap

- ARM64 PE
- Trained Zstd dictionary for sub-MiB binaries
- Polymorphic stub naming
- Optional anti-debug

## license

Dual-licensed under [MIT](LICENSE-MIT) or [Apache-2.0](LICENSE-APACHE) at your option.
