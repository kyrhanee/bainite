```
    __          _       _ __
   / /_  ____ _(_)___  (_) /____
  / __ \/ __ `/ / __ \/ / __/ _ \
 / /_/ / /_/ / / / / / / /_/  __/
/_.___/\__,_/_/_/ /_/_/\__/\___/
```

A modern executable packer for Windows PE (x86_64). Compresses your `.exe`
with LZMA2 at extreme settings and a BCJ x86 filter, then ships it as a
single self-running binary with a small runtime stub.

## what you get

- **Heavy compression.** LZMA2 preset 9 with `nice_len=273` and `depth=999`,
  combined with a BCJ x86 preprocessor.
- **In-memory runtime.** The packed binary maps the original image directly
  into the current process — sections, relocations, imports, page protections,
  TLS callbacks, jump to OEP. No temp files, no `CreateProcess`.
- **Tamper-evident.** BLAKE3 of the payload and of the original image is
  verified at every launch.
- **Three integration modes.** Command-line tool, REST + gRPC daemon, C ABI
  shared library — embed from any language with FFI.

## results

```
bainite.exe       1.38 MiB  →   541 KiB     (-61%)
bainited.exe      3.40 MiB  →  1.26 MiB     (-64%)
real-world app    3.60 MiB  →  1.26 MiB     (-65%)
large native exe 25.10 MiB  →  5.43 MiB     (-78%)
```

`bainite.exe` and `bainited.exe` in the release are themselves packed by bainite.

## download

Latest binaries: **[Releases](../../releases/latest)**.

| file                  | size     | what                          |
| --------------------- | -------: | ----------------------------- |
| `bainite.exe`         | 541 KiB  | command-line packer           |
| `bainited.exe`        | 1.26 MiB | REST + gRPC daemon            |
| `bainite_native.dll`  | 760 KiB  | C ABI shared library          |
| `bainite.h`           |   2 KiB  | C header                      |

Drop `bainite.exe` somewhere on `PATH`.

## quick start

```sh
bainite app.exe              # writes app.bainite.exe (ultra)
bainite -i app.bainite.exe   # show metadata
bainite -t app.bainite.exe   # check if packed (exit 0 / 1)
bainite -b app.exe           # benchmark zstd vs lzma2
```

## cli

```
bainite [OPTIONS] FILE
```

| flag                  | description                                          |
| --------------------- | ---------------------------------------------------- |
| `-o, --output PATH`   | output file (default: `FILE.bainite.exe`)            |
| `-l, --level LEVEL`   | `fast` \| `balanced` \| `max` \| `ultra` (default: `ultra`) |
| `-f, --filter FILTER` | `none` \| `bcj` (default: `bcj`)                     |
| `-i, --info`          | show metadata of a packed binary                     |
| `-t, --test`          | exit 0 if FILE is packed, 1 otherwise                |
| `-b, --bench`         | benchmark compression algorithms on FILE             |
| `-V, --version`       | show version                                         |
| `-h, --help`          | show help                                            |

### examples

```sh
bainite app.exe                   # default lzma2 + bcj + ultra
bainite app.exe -l fast           # quick CI pass
bainite app.exe -l max            # balanced
bainite app.exe -o tiny.exe       # custom output name
bainite -i app.bainite.exe        # metadata
bainite -b app.exe                # head-to-head zstd vs lzma2
```

## daemon

Run the engine over the network — useful for CI or any language that does not
link the FFI.

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
curl -F file=@app.exe -o app.bainite.exe http://localhost:7575/v1/pack
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
4.  hash with BLAKE3          →  payload + original image checksums
5.  emit container            →  192-byte header + manifest + payload
6.  graft into stub PE        →  new ".bainite" section of the stub
```

At launch the embedded stub locates `.bainite`, verifies BLAKE3, decompresses
in memory, restores the image and jumps to the original entry point. Disk is
untouched after the first read.

## supported targets

| target                | status   |
| --------------------- | -------- |
| Windows PE x86_64     | active   |

## roadmap

- Windows PE i686 (32-bit)
- ARM64 PE
- Polymorphic stub naming
- Trained Zstd dictionary in addition to LZMA2

## license

Dual-licensed under [MIT](LICENSE-MIT) or [Apache-2.0](LICENSE-APACHE) at your option.
