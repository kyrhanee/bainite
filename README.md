<div align="center">

```
       __          _       _ __
      / /_  ____ _(_)___  (_) /____
     / __ \/ __ `/ / __ \/ / __/ _ \
    / /_/ / /_/ / / / / / / /_/  __/
   /_.___/\__,_/_/_/ /_/_/\__/\___/
```

### a one-way executable packer for Windows PE

</div>

---

## what's in this archive

| file                 | size      | what                                          |
| -------------------- | --------: | --------------------------------------------- |
| `bainite.exe`        | 510 KiB   | command-line packer (self-packed by bainite)  |
| `bainited.exe`       | 1.20 MiB  | REST + gRPC daemon (self-packed by bainite)   |
| `bainite_native.dll` | 760 KiB   | C ABI shared library                          |
| `bainite.h`          | 2 KiB     | C header for embedding                        |

`bainite.exe` and `bainited.exe` are themselves packed with bainite —
the original 1.38 MiB and 3.40 MiB binaries shrunk to 510 KiB and 1.20 MiB.
That's a 64% reduction; both run normally.

## quick start

```sh
bainite pack app.exe                    # writes app.bainite.exe
bainite info app.bainite.exe            # shows metadata
bainite bench app.exe                   # zstd vs lzma2 ratio table
```

bainite is a **one-way packer**: it compresses, it does not extract.
Once a binary is packed, the original bytes cannot be recovered from it —
only the embedded runtime stub can decompress in memory to execute.

## benchmarks vs UPX

```
file                       original       bainite        upx       winner
hello       (124 KiB)        121 KiB     70 KiB       57 KiB    upx -13 KiB
real-app    (3.6 MiB)      3 690 KiB  1 292 KiB    1 235 KiB    upx -57 KiB
ntoskrnl   (10.4 MiB)     10 614 KiB  4 339 KiB         FAIL   bainite (UPX cannot)
nvidia-pcc (25.1 MiB)     25 740 KiB  5 557 KiB    5 845 KiB   bainite +289 KiB
```

bainite wins on large binaries and handles native-subsystem images
that UPX rejects.

## cli

```
bainite pack    <input> [-o out] [--level fast|balanced|max|ultra]
                       [--algo lzma2|zstd|stored|auto]
                       [--filter none|bcj|bcj2]
                       [--encrypt] [--stub <path>]

bainite info    <input> [--json]
bainite bench   <input> [--level N]
```

Defaults: `--level ultra --algo lzma2 --filter bcj`.

## daemon

```sh
bainited --bind 127.0.0.1:7575 --grpc-bind 127.0.0.1:7576 --token SECRET
```

REST endpoints: `GET /healthz`, `POST /v1/pack`, `POST /v1/info`.
gRPC service: `bainite.v1.Packer`.

## license

dual-licensed under [MIT](LICENSE-MIT) or [Apache-2.0](LICENSE-APACHE).
