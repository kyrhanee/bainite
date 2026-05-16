<div align="center">
<pre>
   __          _       _ __
  / /_  ____ _(_)___  (_) /____
 / __ \/ __ `/ / __ \/ / __/ _ \
/ /_/ / /_/ / / / / / / /_/  __/
/_.___/\__,_/_/_/ /_/_/\__/\___/
</pre>

### a one-way executable packer for Windows PE

[![License](https://img.shields.io/badge/license-MIT_%2F_Apache--2.0-blue?style=flat-square)](#license)
[![Platform](https://img.shields.io/badge/platform-windows-lightgrey?style=flat-square)](#supported-targets)
[![Engine](https://img.shields.io/badge/engine-LZMA2_extreme-red?style=flat-square)](#how-it-works)
[![Stub](https://img.shields.io/badge/stub-16_KiB-green?style=flat-square)](#how-it-works)

</div>

---

## what

**bainite** shrinks Windows PE executables further than UPX on real-world
binaries. It uses LZMA2 at extreme settings, a BCJ x86 filter, and an
**in-memory** runtime — no temp files, no `CreateProcess`, no extra disk I/O.

> bainite is **one-way**. It compresses; it does not extract.
> There is no `unpack` command, no `unpack` endpoint, no FFI to recover
> the original bytes. Once a binary is packed, only the embedded runtime stub
> can decompress it into memory to execute it. Your code stays your code.

## features

- **LZMA2 extreme** (preset 9, `nice_len=273`, `depth=999`) for maximum ratio
- **BCJ x86** preprocessing — relative `call`/`jmp` becomes absolute, gives the
  entropy coder a chance to find long matches
- **Adaptive strategy** — `--algo auto` benchmarks Zstd vs LZMA2 and picks
  whichever produces the smaller payload
- **In-memory unpacking** — sections, relocations, imports, page protections,
  TLS callbacks, then a direct jump to OEP. Disk untouched after launch.
- **16 KiB stub** — overhead disappears on binaries ≥ 5 MiB
- **REST + gRPC daemon** for CI / packaging pipelines
- **C ABI** for embedding from Go, Python, C#, Java, Node.js
- **Native subsystem** support — packs `ntoskrnl`-style PEs that UPX rejects

## benchmarks vs UPX 4.2.4

```
file                       original       bainite        upx       winner
hello       (124 KiB)        121 KiB     70 KiB       57 KiB    upx     -13 KiB
real-app    (3.6 MiB)      3 690 KiB  1 292 KiB    1 235 KiB    upx     -57 KiB
ntoskrnl   (10.4 MiB)     10 614 KiB  4 339 KiB         FAIL    bainite (UPX cannot)
nvidia-pcc (25.1 MiB)     25 740 KiB  5 557 KiB    5 845 KiB    bainite +289 KiB
```

| input | bainite ratio | upx ratio | bainite vs upx |
| ----- | ------------: | --------: | -------------: |
| hello       | 42.0% saved  | 53.0% saved | UPX better on tiny files (stub overhead) |
| real-app    | 65.0% saved  | 66.5% saved | within 2% |
| ntoskrnl    | 59.1% saved  | —          | **only bainite handles native subsystem** |
| nvidia-pcc  | 78.4% saved  | 77.3% saved | **bainite wins on large files** |

**bainite.exe and bainited.exe in this repo are themselves packed by bainite:**

```
bainite.exe   1.38 MiB → 510 KiB   (-64%)
bainited.exe  3.40 MiB → 1.20 MiB  (-65%)
```

## install

Grab from this repo's [Releases](../../releases/latest), or just download
the EXEs from the repo root and put them somewhere on `PATH`.

| file                    | size    | what                                          |
| ----------------------- | ------: | --------------------------------------------- |
| `bainite.exe`           | 510 KiB | command-line packer (self-packed)             |
| `bainited.exe`          | 1.20 MiB| REST + gRPC daemon (self-packed)              |
| `bainite_native.dll`    | 760 KiB | C ABI shared library                          |
| `bainite.h`             |   2 KiB | C header                                      |

## quick start

```sh
bainite pack  app.exe                    # writes app.bainite.exe
bainite info  app.bainite.exe            # shows metadata
bainite bench app.exe                    # zstd vs lzma2 ratio table
```

That's it. Defaults are tuned: `--level ultra --algo lzma2 --filter bcj`.

## cli reference

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

| option        | default | what it does                                          |
| ------------- | :-----: | ----------------------------------------------------- |
| `--level`     | `ultra` | preset for the chosen algorithm                       |
| `--algo`      | `lzma2` | which compressor; `auto` tries Zstd and LZMA2 both    |
| `--filter`    | `bcj`   | x86 preprocessor: `bcj` (classic) or `bcj2` (split)   |
| `--encrypt`   | off     | apply ChaCha20 over the compressed payload            |
| `--stub`      | embedded| path to a custom stub PE                              |

### examples

```sh
bainite pack app.exe                                  # default lzma2 + bcj + ultra
bainite pack app.exe --level fast                     # quick pass for CI
bainite pack app.exe --algo auto --filter bcj2        # let the engine pick
bainite pack app.exe --encrypt                        # opaque packed bytes
bainite info app.bainite.exe --json                   # machine-readable
bainite bench big.exe --level 22                      # see Zstd vs LZMA2 head-to-head
```

## daemon

Run the engine over the network — useful for CI, packaging pipelines, or
languages that don't link the FFI.

```sh
bainited --bind 127.0.0.1:7575 --grpc-bind 127.0.0.1:7576 --token SECRET
```

By default `bainited` binds to `127.0.0.1` (loopback) and refuses any other
host without `--token`.

### REST

| Method | Path             | Body                          | Returns           |
| ------ | ---------------- | ----------------------------- | ----------------- |
| `GET`  | `/healthz`       | —                             | `{status,version}` |
| `POST` | `/v1/pack`       | multipart `file`, `options`   | packed binary     |
| `POST` | `/v1/info`       | multipart `file`              | JSON metadata     |

```sh
curl -F file=@app.exe \
     -F 'options={"algorithm":"lzma2","level":22,"filter":"bcj"}' \
     -o app.bainite.exe \
     http://localhost:7575/v1/pack
```

With token:

```sh
curl -H "Authorization: Bearer SECRET" \
     -F file=@app.exe \
     -o app.bainite.exe \
     http://localhost:7575/v1/pack
```

### gRPC

Service `bainite.v1.Packer` with three RPCs: `Pack`, `Info`, `Health`. Generate
clients for any language from the `.proto` schema.

## C ABI (embedding)

Header: `bainite.h`. Link against `bainite_native.dll` (and `bainite_native.lib`
for the import table on MSVC).

```c
#include "bainite.h"

int main(void) {
    BainiteOptions opts = bainite_default_options();
    BainiteReport  rep  = {0};

    int rc = bainite_pack_file("app.exe", "app.bainite.exe", &opts, &rep);
    if (rc != BAINITE_OK) {
        char err[256];
        bainite_last_error(err, sizeof err);
        fprintf(stderr, "pack failed: %s\n", err);
        return 1;
    }
    printf("%llu -> %llu bytes\n", rep.original_size, rep.packed_size);
    return 0;
}
```

Functions:

| function                      | purpose                                       |
| ----------------------------- | --------------------------------------------- |
| `bainite_version()`           | get version string                            |
| `bainite_default_options()`   | sane defaults (LZMA2, BCJ, level 22)          |
| `bainite_pack_file(...)`      | pack from path to path                        |
| `bainite_pack_buffer(...)`    | pack from memory to memory                    |
| `bainite_is_packed(path)`     | non-zero if the file is a bainite container   |
| `bainite_inspect(path, out)`  | populate a `BainiteReport` from metadata only |
| `bainite_last_error(buf, n)`  | retrieve the last error message              |

Works from Go (cgo), Python (ctypes), C#/.NET, Java (JNA), Node.js
(ffi-napi), or anything else with FFI.

## how it works

### at pack time

```
1.  parse PE                  ─→  sections, imports, relocs, OEP, image base
2.  apply BCJ x86             ─→  relative call/jmp → absolute targets
3.  compress with LZMA2 e9    ─→  preset 9 + extreme (nice_len 273, depth 999)
4.  optional ChaCha20         ─→  scramble the payload
5.  emit container            ─→  192-byte header + manifest + payload
6.  graft into stub PE        ─→  new section ".bainite" of the stub
```

### at run time

```
1.  GetModuleHandle(NULL)     ─→  locate self image
2.  scan section table        ─→  find ".bainite"
3.  parse header              ─→  algorithm, filter, sizes, checksums
4.  decompress (LZMA2)        ─→  in-memory only
5.  reverse BCJ filter        ─→  restore original bytes
6.  VirtualAlloc(image_base)  ─→  reserve at preferred base when possible
7.  copy sections             ─→  by virtual address
8.  apply relocations         ─→  if delta != 0
9.  resolve imports           ─→  LoadLibraryA + GetProcAddress
10. VirtualProtect            ─→  per-section X/W/R flags
11. invoke TLS callbacks      ─→  DLL_PROCESS_ATTACH
12. FlushInstructionCache     ─→  flush the new code
13. jump to entry point       ─→  the original OEP
```

No file is ever written to disk during launch. Process Monitor sees one
read of the packed EXE and zero further file activity.

## supported targets

| target                | pack | stub        | status   |
| --------------------- | :--: | ----------- | -------- |
| Windows PE x86_64     | yes  | in-memory   | active   |
| Windows PE i686       | yes  | in-memory   | active   |

## roadmap

- ARM64 PE
- Trained Zstd dictionary for sub-MiB binaries (closes the gap with UPX on tiny files)
- Polymorphic stub naming and section randomization
- Optional anti-debug

## license

dual-licensed under [MIT](LICENSE-MIT) or [Apache-2.0](LICENSE-APACHE) at your
option. Use whichever fits your project.
