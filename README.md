# Bainite

```
   __          _       _ __
  / /_  ____ _(_)___  (_) /____
 / __ \/ __ `/ / __ \/ / __/ _ \
/ /_/ / /_/ / / / / / / /_/  __/
/_.___/\__,_/_/_/ /_/_/\__/\___/
```

![Build](https://img.shields.io/badge/build-passing-brightgreen)
![License](https://img.shields.io/badge/license-MIT%20%7C%20Apache--2.0-blue)
![Platform](https://img.shields.io/badge/platform-Windows-lightgrey)
![Compression](https://img.shields.io/badge/compression-up%20to%2078%25-red)

A modern Windows `.exe` packer. Compresses executables with LZMA2 at extreme
settings, ships as a single binary, runs the original program transparently.

## ⚙️ Features

- 📦 **Heavy compression** — up to **78%** size reduction (LZMA2 preset 9 + extreme + BCJ x86 filter)
- ⚡ **Parallel** — multi-threaded LZMA2 on files ≥ 1 MiB, 2–3× faster on multi-core CPUs
- 🧠 **Adaptive dictionary** — 16 / 64 / 128 MiB depending on input size
- 🪒 **Overlay strip** — removes signature/cert/padding bytes before compression
- 🛡️ **Tamper-evident** — BLAKE3 of payload and original image verified at every launch
- ✅ **Wide compatibility** — works on every PE that runs natively, including Node.js, Electron, V8 apps
- 🧩 **Three integration modes** — CLI, native shared library (DLL), self-hosted REST + gRPC daemon
- 🪟 **Windows 10 / 11 manifest** — embedded `supportedOS` + UTF-8 + long path aware

## 📥 Download

Pre-built binaries: **[Releases](../../releases/latest)**

| File                  | What                                  |
| --------------------- | ------------------------------------- |
| `bainite.exe`         | Command-line packer                   |
| `bainited.exe`        | REST + gRPC daemon                    |
| `bainite_native.dll`  | Native shared library (FFI)           |
| `bainite.h`           | C header for the shared library       |

## 🚀 Quick start

```
bainite app.exe
```

Packs `app.exe` into `app.bainite.exe`. Run it like the original — same behaviour, much smaller.

## 🖥️ Usage

```
bainite [OPTIONS] FILE
```

| Flag                  | Description                                          |
| --------------------- | ---------------------------------------------------- |
| `-o, --output PATH`   | Output file (default: `FILE.bainite.exe`)            |
| `-l, --level LEVEL`   | `fast` \| `balanced` \| `max` \| `ultra` (default: `ultra`) |
| `-f, --filter FILTER` | `none` \| `bcj` (default: `bcj`)                     |
| `--keep-overlay`      | Don't strip overlay bytes (signed installers etc)    |
| `-i, --info`          | Show file info / metadata                            |
| `-t, --test`          | Exit `0` if FILE is packed, `1` otherwise            |
| `-b, --bench`         | Benchmark zstd vs lzma2 on FILE                      |
| `-V, --version`       | Print build info                                     |
| `-h, --help`          | Show help                                            |

### Examples

```
bainite app.exe                   # pack into app.bainite.exe (ultra)
bainite app.exe -l fast           # fast pack — seconds
bainite app.exe -l max            # balanced
bainite app.exe -o tiny.exe       # custom output name
bainite -i app.bainite.exe        # show metadata
bainite -t app.bainite.exe        # check if packed (exit 0/1)
bainite -b app.exe                # benchmark
```

### Example output

```
$ bainite node.exe -l max

  [*] input          node.exe
  [*] output         node.bainite.exe
  [*] algorithm      lzma2 L19
  [*] filter         bcj-x86
  [*] original       87.43 MiB
  [*] packed         21.38 MiB
  [*] saved          66.05 MiB (75.54%)
  [*] elapsed        47218 ms

  [+] packed successfully
```

## 📊 Compression results

Real-world benchmark on a 87.43 MiB Node.js v24 binary:

| Level     | Size       | Reduction | Time   |
| --------- | ---------- | --------- | ------ |
| `fast`    | 23.68 MiB  | 72.92%    | 7 s    |
| `max`     | 21.38 MiB  | 75.54%    | 47 s   |
| `ultra`   | **21.32 MiB** | **75.62%** | 48 s |

## 📚 Native library (DLL)

For embedding from Go (cgo), Python (ctypes), C/C++, or any language with FFI.
Header in `bainite.h`:

```c
const char*    bainite_version(void);
BainiteOptions bainite_default_options(void);

int32_t bainite_pack_file(const char* in, const char* out,
                          const BainiteOptions* opts, BainiteReport* report);
int32_t bainite_pack_buffer(const uint8_t* in, size_t in_len,
                            const BainiteOptions* opts,
                            uint8_t* out, size_t cap, size_t* out_len,
                            BainiteReport* report);
int32_t bainite_is_packed(const char* path);
int32_t bainite_inspect(const char* path, BainiteReport* report);
int32_t bainite_last_error(char* buf, size_t buf_len);
```

Returns `0` on success, negative on error.

## 🌐 REST + gRPC daemon

For remote / automated packing — run `bainited.exe`:

```
bainited                                        # listens on 127.0.0.1:7575 (loopback)
bainited --bind 0.0.0.0:7575 --token SECRET     # exposed; token mandatory
bainited --grpc-bind 127.0.0.1:7576             # also start gRPC
```

By default the server binds to `127.0.0.1` (loopback only). Binding to a
non-loopback address requires `--token`, otherwise startup is refused.

REST endpoints:

| Method | Path        | Body                          | Returns           |
| ------ | ----------- | ----------------------------- | ----------------- |
| `GET`  | `/healthz`  | —                             | `{"status":"ok"}` |
| `POST` | `/v1/pack`  | multipart `file=`, optional `options=` | packed PE binary |
| `POST` | `/v1/info`  | multipart `file=`             | JSON metadata     |

```
curl -F file=@app.exe -F 'options={"level":22}' \
     -o app.bainite.exe \
     http://localhost:7575/v1/pack
```

When a token is set:

```
curl -H "Authorization: Bearer SECRET" -F file=@app.exe \
     -o app.bainite.exe \
     http://your-host:7575/v1/pack
```

gRPC: service `bainite.v1.Packer` with three RPCs — `Pack`, `Info`, `Health`.

## 🖥️ Compatibility

- ✅ Windows 10 / 11 (x64)
- ✅ Windows Server 2019 / 2022
- ✅ Console and GUI subsystem inputs
- ✅ Native subsystem (kernel-style) inputs
- ✅ Complex runtimes — Node.js, Electron, V8-based apps
- ⏳ x86 (32-bit) — planned
- ⏳ ARM64 — planned

## 📜 License

Dual-licensed under [MIT](LICENSE-MIT) or [Apache-2.0](LICENSE-APACHE).

For legitimate software-distribution purposes only.
