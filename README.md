<h1>
  <img src="web/src/assets/logos/camflow_icon.svg" alt="camflow icon" width="64" align="absmiddle" />
  camflow v0.2.0
</h1>

camflow contains one application:

- `camflow`: Linux runtime for camera pipelines on x86_64 and ARM64 targets.

The runtime uses OpenCV 4.12 and links the needed OpenCV modules statically.

## Documentation

- [Specification](docs/development/specification.md)
- [Architecture](docs/development/architecture.md)
- [Nodes](docs/release/nodes.md)
- [FrameContext](docs/development/frame_context.md)
- [Scheduler](docs/development/scheduler.md)
- [Image Conversion](docs/development/image_conversion.md)
- [JSON Graph Format](docs/release/graph_json.md)
- [REST API](docs/release/rest_api.md)
- [Web UI Service](docs/development/web_ui.md)
- [Driver Release Test Automation Use Case](docs/development/driver_release_test_automation.md)
- [Web UI Design](docs/ui_design/ui_design.md)
- [Documentation Guide](docs/development/documentation.md)
- [Debugging](docs/development/debugging.md)
- [Development Guide](docs/development/development.md)

## Runtime build

Recommended workflow:

```bash
./scripts/tasks.sh build dev runtime
```

Direct CMake build (advanced/manual path):

```bash
cmake -B build-runtime -DCMAKE_BUILD_TYPE=Release -DBUILD_RUNTIME=ON
cmake --build build-runtime --parallel
```

The runtime is supported only on Linux x86_64 and Linux ARM64.

OpenCV static artifacts are generated in versioned folders under `opencv/`, for example `opencv/4.12.0/`, with per-architecture install and build directories.

## Task Script

`scripts/tasks.sh` is the central task entry point.

- On first run, if `scripts/tasks.cfg` is missing, the script creates it.
- In interactive shells it asks for values (target host, port, OpenCV version, camera devices, and more).
- In non-interactive runs it writes `scripts/tasks.cfg` with defaults.

Show help:

```bash
./scripts/tasks.sh --help
```

Typical examples:

```bash
./scripts/tasks.sh deps --opencv 4.12.0
./scripts/tasks.sh format --clang-format clang-format-15
./scripts/tasks.sh docs --clean --install-deps
./scripts/tasks.sh build dev ui
./scripts/tasks.sh build dev runtime
./scripts/tasks.sh build arm64 runtime --opencv 4.12.0
./scripts/tasks.sh build deploy run arm64 runtime --ip imx8mp-var-dart --dir /usr/bin --user root --port 8000
```

Action summary:

- `deps`: Install host packages and project-local OpenCV artifacts.
- `format`: Format C/C++ sources with clang-format.
- `docs`: Generate API docs with Doxygen and doxygen-awesome.
- `build`: Build selected products (`runtime`, `ui`) for selected platforms (`dev`, `arm64`).
- `deploy`: Deploy ARM64 runtime binary to target.
- `run`: Start local or remote UI-mode runtime.

## VS Code Task Workflows

Recommended workflows in `.vscode/tasks.json`:

### Local development (runtime on 8000, UI on 8081)

1. Run task: `Camflow: Build and restart runtime (local)`
2. Run task: `Camflow: Run UI dev server (local)`
3. Open: `http://localhost:8081`

The local UI task proxies to `127.0.0.1:${UI_API_PORT}` from `scripts/tasks.cfg`.
Recommended default is `UI_API_PORT=8000`.

WebSocket behavior in local dev mode:

- When UI runs on port `8081`, frame websocket resolves to the same proxy target.

### Remote development (runtime on ${TARGET}:8000, UI on 8081)

1. Run task: `Camflow: Build and restart runtime (remote)`
2. Run task: `Camflow: Run UI dev server (remote)`
3. Open: `http://localhost:8081`

The remote tasks read host and port from `scripts/tasks.cfg`:

- `TARGET` controls the remote host (for example `pollux` or `imx8mp-var-dart`)
- `PORT` controls runtime UI/REST port (default `8000`)

This remote UI task sets:

- `CAMFLOW_WEB_API_TARGET=http://${TARGET}:${PORT}`
- `VITE_CAMFLOW_WS_TARGET=ws://${TARGET}:${PORT}`

To switch remote targets, edit only `TARGET` in `scripts/tasks.cfg` and rerun the
two remote tasks.

Important:

- Start only one UI dev server on port `8081` at a time.
- If startup fails with `Port 8081 is already in use`, stop the running Vite process first.

## UI mode

```bash
camflow --port 8000 --device /dev/video3 --subdevices /dev/v4l-subdev0,/dev/v4l-subdev3
```

Open `http://<target-ip>:8000` in a browser. The page shows the live image stream and a parameter browser for `v4l2src`.
UI mode starts stopped, so `device` and `subdevices` can be set first in the UI.
The UI header also uses the camflow icon to match CLI and documentation branding.

## ARM64 build and deploy workflow

Build ARM64 runtime locally via Docker cross-compilation:

```bash
./scripts/tasks.sh build arm64 runtime [--opencv OPENCV_VERSION]
```Deploy binary to target:

```bash
./scripts/tasks.sh deploy arm64 runtime [--ip TARGET] [--dir TARGET_DIR] [--user USER]
```

Run UI mode on target:

```bash
./scripts/tasks.sh run arm64 runtime [--ip TARGET] [--dir TARGET_DIR] [--port PORT] [--user USER]
```

One-command workflow (build + deploy + start UI):

```bash
./scripts/tasks.sh build deploy run arm64 runtime [--ip TARGET] [--dir TARGET_DIR] [--port PORT] [--user USER] [--opencv OPENCV_VERSION]
```

Default deployment command:

```bash
./scripts/tasks.sh deploy arm64 runtime --ip imx8mp-var-dart --dir /usr/bin --user root
./scripts/tasks.sh run arm64 runtime --ip imx8mp-var-dart --dir /usr/bin --port 8000 --user root
```

Then open `http://<target-ip>:<PORT>` in your browser (including the VS Code browser) to immediately verify changes.

### Supported ARM64 targets and OS requirements

The `arm64` build produces a single binary built against `docker/ubuntu-arm64/Dockerfile`
(Ubuntu 20.04, glibc 2.31, libstdc++ 3.4.28, statically linked into the runtime via
`-static-libgcc -static-libstdc++`). Since glibc is backward- but not forward-compatible,
that binary runs on any target with glibc >= 2.31 (64-bit OS required):

| Target family | Minimum OS | glibc | Verified |
| --- | --- | --- | --- |
| NXP i.MX (8M Plus and similar arm64 i.MX SoMs) | Yocto/Debian-based 64-bit OS, glibc >= 2.31 | 2.39 (imx8mp-var-dart, current) | yes (imx8mp-var-dart) |
| Raspberry Pi 3/4/5 | Raspberry Pi OS Bullseye (Debian 11) 64-bit or newer | 2.31 (Bullseye) / 2.36 (Bookworm, current) | yes (Pi 5, Bookworm) |
| NVIDIA Jetson Xavier NX / AGX Xavier | JetPack 5.x (L4T 34/35, Ubuntu 20.04) — last JetPack line with Xavier support | 2.31 | no |
| NVIDIA Jetson Orin / Thor | JetPack 6.x+ (L4T 36+, Ubuntu 22.04+) | 2.35+ | no |
| NVIDIA Jetson Nano / TX1 / TX2 | JetPack 4.x (L4T 32.x, Ubuntu 18.04) | 2.27 | not supported (needs a separate legacy build) |
| Raspberry Pi 3/4 on 32-bit OS, or Raspberry Pi OS Buster (Debian 10) | — | 2.28 (Buster) / n/a (32-bit) | not supported (needs a separate legacy or `armhf` build) |

Check a target's compatibility before deploying:

```bash
uname -m       # must be aarch64/arm64, not armv7l/armv6l
ldd --version  # glibc must be >= 2.31
```

If a target's glibc is older than 2.31 (or it runs a 32-bit OS), it needs a separate
build with a correspondingly older/other cross-build image; a single arm64 binary
cannot cover it.

## Runtime use case

camflow can process pipeline expressions and JSON graph files:

```text
v4l2src(device=/dev/video0,fourcc=RG10) -> tcpsink(ip=<host-ip>,port=9000)
```

## Test and driver development use case

The runtime can still be scripted from the CLI for camera driver development:

```bash
camflow --graph tests/graphs/viewer_v4l2_to_gui.json
camflow -G tests/graphs/viewer_v4l2_to_gui.json
camflow -n 100 "v4l2src(device=/dev/video0,fourcc=RG10)->tcpsink(ip=127.0.0.1,port=9000)"
camflow -p -n 200 "v4l2src(device=/dev/video0)->logsink"
camflow -s -p -n 200 "v4l2src(device=/dev/video0)->logsink"
```

Console output shows node logs by default. Select additional sources with
`-L`/`--log-source`; this does not filter the web UI log stream:

```bash
camflow -L node,api "v4l2src(device=/dev/video0)->logsink"
camflow -L all,-kernel PIPELINE
```

If no `-G`/`--graph` option is provided, a pipeline string argument is required.

Use `-n MAX_FRAMES` to stop the runtime after a fixed number of processed frames (`0` keeps running).

Use `-p` or `--profile` to enable per-node execution profiling. At the end of the run, the runtime prints a report with call count, failures, and min/avg/max/total execution times for each node plus a `TOTAL` summary across all nodes. Timing values are printed with 3 decimal places in milliseconds.

Use `-s` or `--simple-pipeline` to force linear execution with `PipelineSimple`. Graph edges are not used for scheduling in this mode, but are still available for default FrameContext scope inference.

Pipelines may capture images with different camera parameters, store them with `FileSink`, send them to network peers with `TCPSink`, and use processors to evaluate image content.
