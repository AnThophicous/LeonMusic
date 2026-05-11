# LeonMusic

LeonMusic is an open-source cross-platform shared library and CLI for a serious music application stack in C++.

This repository is being built as an engineering experiment developed by Codex from OpenAI to test how far an AI agent can code a production-oriented foundation without direct human implementation.

## Scope

- Shared library for Windows, Linux, and macOS
- CLI surface exposed as `lma`
- Cross-platform IPC surface:
  - stdio JSON-RPC
  - named pipes on Windows
  - Unix sockets on Linux and macOS
- HTTP integrations designed for libcurl
- JSON contracts built around `nlohmann/json`
- Music metadata pipeline with TagLib support and `ffprobe` fallback
- Audio playback and DSP pipeline using `miniaudio`
- YouTube search and resolve pipeline powered by `yt-dlp`
- Optional Discord Rich Presence, locked to internal playback state only

## Security note for Discord RPC

Discord Rich Presence is intentionally restricted:

- The CLI does not expose a command to set arbitrary Discord text
- RPC presence is derived only from the active internal playback state
- Risky metadata is neutralized before publication
- Stopped playback clears active listening details

This prevents the project API from being used as a generic status spoofer.

## Current state

The repository currently provides:

- Core project scaffold with a public C++ API
- Shared-library C API for external consumers
- `lma` CLI command surface
- Chained CLI execution inside one process for queue control
- `--wait-until-finished` for terminal playback sessions
- `pause`, `resume`, and volume control from CLI / API / JSON-RPC
- stdio JSON-RPC server
- Real YouTube text search, ID resolve and URL resolve through `yt-dlp`
- Cached YouTube audio extraction to local files
- Local audio playback using `miniaudio`
- Metadata extraction from local/downloaded files with TagLib support and `ffprobe` fallback
- In-memory playback queue with next / previous / clear
- 8D / spatial playback trigger based on title and metadata markers
- Discord RPC security policy and integration seam

The following are scaffolded and intentionally left behind interfaces for the next slices:

- Named pipe / Unix socket server runtime
- Full Discord IPC transport implementation
- Richer 8D/HRTF processing beyond positional motion

## Build

### Windows

```powershell
cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=clang++
cmake --build build
```

### Linux / macOS

```bash
cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=clang++
cmake --build build
```

## CLI examples

```bash
lma --search-music "dQw4w9WgXcQ"
lma --play-music "./song1.wav" --enqueue-music "./song2.wav" --wait-until-finished
lma --play-music "./song1.wav" --set-volume 0.6 --pause-music --resume-music
lma --play-music "https://www.youtube.com/watch?v=dQw4w9WgXcQ"
lma --playback-state
lma --serve stdio-jsonrpc
```

## Runtime notes

- YouTube search and download currently depend on `yt-dlp`
- Metadata uses TagLib when available; on this Windows toolchain the build currently falls back to `ffprobe`
- Some YouTube videos can still fail with upstream `403` restrictions depending on extractor state, region, or cookies
- LeonMusic now retries multiple YouTube client profiles automatically to reduce `403`
- LeonMusic prefetches queued tracks in the background when the process stays alive
- For harder videos, you can provide browser cookies:

```powershell
$env:LEONMUSIC_YTDLP_COOKIES_FROM_BROWSER="chrome"
.\build\lma.exe --play-music "https://www.youtube.com/watch?v=..."
```

- Or provide a cookies file:

```powershell
$env:LEONMUSIC_YTDLP_COOKIES="C:\path\to\cookies.txt"
.\build\lma.exe --play-music "https://www.youtube.com/watch?v=..."
```

- Speed knobs for slower connections / faster startup:

```powershell
$env:LEONMUSIC_YTDLP_CONCURRENT_FRAGMENTS="8"
$env:LEONMUSIC_YTDLP_AUDIO_QUALITY="4"
```

## .NET 8 integration

The project may include .NET 8 helpers in later slices for isolated performance-sensitive or platform-specific tasks. Any .NET executable shipped here must be published self-contained, so end users never need to install .NET manually.

Reference publish model:

```powershell
dotnet publish src/Helper/Helper.csproj -c Release -r win-x64 --self-contained true /p:PublishSingleFile=true
```
