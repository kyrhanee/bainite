```
   __          _       _ __
  / /_  ____ _(_)___  (_) /____
 / __ \/ __ `/ / __ \/ / __/ _ \
/ /_/ / /_/ / / / / / / /_/  __/
/_.___/\__,_/_/_/ /_/_/\__/\___/
```

A one-way executable packer for Windows PE.

`bainite` shrinks Windows EXEs further than UPX on real-world binaries. It uses
LZMA2 at extreme settings, a BCJ x86 filter, and an in-memory runtime — no
temporary files, no `CreateProcess`.

> **One-way.** It compresses; it does not extract. There is no `unpack`
> command, no `unpack` endpoint, no FFI to recover the original bytes. Only
> the embedded runtime stub can decompress in memory to execute. Your code
> stays your code.

## benchmarks vs UPX 4.2.4

```
file                       original       bainite        upx       winner
hello       (124 KiB)        121 KiB     70 KiB       57 KiB    upx     -13 KiB
real-app    (3.6 MiB)      3 690 KiB  1 292 KiB    1 235 KiB    upx     -57 KiB
ntoskrnl   (10.4 MiB)     10 614 KiB  4 339 KiB         FAIL    bainite (UPX cannot)
nvidia-pcc (25.1 MiB)     25 740 KiB  5 557 KiB    5 845 KiB    bainite +289 KiB
```

bainite wins on large binaries and packs native-subsystem images that UPX
rejects. Stub is **16 KiB** — overhead disappears on files of 5 MiB and up.

`bainite.exe` and `bainited.exe` in this repo are themselves packed by bainite:

```
bainite.exe   1.38 MiB → 510 KiB  (-64%)
bainited.exe  3.40 MiB → 1.20 MiB (-65%)
```

## install

Grab the latest from [**Releases**](../../releases/latest):

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
bainite bench app.exe              # compares zstd vs lzma2
```

Defaults: `--level ultra --algo lzma2 --filter bcj` — what produced the table above.

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

| option       | default | what                                                |
| ------------ | :-----: | --------------------------------------------------- |
| `--level`    | `ultra` | preset for the chosen algorithm                     |
| `--algo`     | `lzma2` | `auto` benchmarks Zstd vs LZMA2 and picks the best  |
| `--filter`   | `bcj`   | x86 preprocessor: `bcj` (classic) or `bcj2` (split) |
| `--encrypt`  | off     | apply ChaCha20 over the compressed payload          |
| `--stub`     | embedded| path to a custom stub PE                            |

### examples

```sh
bainite pack app.exe                                 # default lzma2 + bcj + ultra
bainite pack app.exe --level fast                    # quick CI pass
bainite pack app.exe --algo auto --filter bcj2       # let engine pick
bainite pack app.exe --encrypt                       # opaque payload
bainite info app.bainite.exe --json                  # machine-readable
bainite bench big.exe --level 22                     # head-to-head
```

## daemon (REST + gRPC)

```sh
bainited --bind 127.0.0.1:7575 --grpc-bind 127.0.0.1:7576 --token SECRET
```

By default `bainited` binds to `127.0.0.1` and refuses non-loopback hosts
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

**At pack time:**

1. Parse the PE — sections, imports, relocations, OEP, image base
2. Apply BCJ x86 — convert relative `call`/`jmp` to absolute targets
3. Compress with LZMA2 preset 9 + extreme (`nice_len=273`, `depth=999`)
4. Optionally encrypt the payload with ChaCha20
5. Emit a 192-byte header + manifest + payload
6. Graft the result into a new `.bainite` section of the stub PE

**At run time:**

1. Locate `.bainite` in the running image
2. Decompress LZMA2 in memory, reverse BCJ
3. `VirtualAlloc` at the preferred image base
4. Copy sections by virtual address
5. Apply relocations if the base shifted
6. Resolve imports via `LoadLibraryA` + `GetProcAddress`
7. `VirtualProtect` per-section X/W/R flags
8. Invoke TLS callbacks with `DLL_PROCESS_ATTACH`
9. `FlushInstructionCache`, jump to the original entry point

Disk is untouched after launch.

## supported targets

| target                | pack | stub        | status   |
| --------------------- | :--: | ----------- | -------- |
| Windows PE x86_64     | yes  | in-memory   | active   |
| Windows PE i686       | yes  | in-memory   | active   |

## roadmap

- ARM64 PE
- Trained Zstd dictionary for sub-MiB binaries
- Polymorphic stub naming
- Optional anti-debug

## license

Dual-licensed under [MIT](LICENSE-MIT) or [Apache-2.0](LICENSE-APACHE) at your option.
