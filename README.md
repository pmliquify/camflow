<h1>
  <img src="web/src/assets/logos/camflow_icon.svg" alt="camflow icon" width="64" align="absmiddle" />
  camflow v0.1.0
</h1>

camflow contains one application:

- `camflow`: Linux runtime for camera pipelines on x86_64 and ARM64 targets.

The runtime uses OpenCV 4.12 and links the needed OpenCV modules statically.

## Documentation

- [Specification](docs/specification.md)
- [Architecture](docs/architecture.md)
- [Nodes](docs/nodes.md)
- [FrameContext](docs/frame_context.md)
- [Scheduler](docs/scheduler.md)
- [Image Conversion](docs/image_conversion.md)
- [JSON Graph Format](docs/graph_json.md)
- [REST API](docs/rest_api.md)
- [Web UI Service](docs/web_ui.md)
- [Driver Release Test Automation Use Case](docs/driver_release_test_automation.md)
- [Web UI Design](docs/ui_design.md)
- [Documentation Guide](docs/documentation.md)
- [Debugging](docs/debugging.md)
- [Development Guide](docs/development.md)

## Runtime build

Recommended workflow:

```bash
./scripts/tasks.sh build dev runtime
```

Direct CMake build (advanced/manual path):

```bash
cmake -B build-runtime -DBUILD_RUNTIME=ON
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
./scripts/tasks.sh build arm64 runtime --release --opencv 4.12.0
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
camflow --port 8000 --device /dev/video3 --subdevice /dev/v4l-subdev3
```

Open `http://<target-ip>:8000` in a browser. The page shows the live image stream and a parameter browser for `v4l2src`.
UI mode starts stopped, so `device` and `subdevice` can be set first in the UI.
The UI header also uses the camflow icon to match CLI and documentation branding.

## ARM64 build and deploy workflow

Build ARM64 runtime locally via Docker cross-compilation:

```bash
./scripts/tasks.sh build arm64 runtime [--release] [--opencv OPENCV_VERSION]
```

Deploy binary to target:

```bash
./scripts/tasks.sh deploy arm64 runtime [--ip TARGET] [--dir TARGET_DIR] [--user USER]
```

Run UI mode on target:

```bash
./scripts/tasks.sh run arm64 runtime [--ip TARGET] [--dir TARGET_DIR] [--port PORT] [--user USER]
```

One-command workflow (build + deploy + start UI):

```bash
./scripts/tasks.sh build deploy run arm64 runtime [--release] [--ip TARGET] [--dir TARGET_DIR] [--port PORT] [--user USER] [--opencv OPENCV_VERSION]
```

Default deployment command:

```bash
./scripts/tasks.sh deploy arm64 runtime --ip imx8mp-var-dart --dir /usr/bin --user root
./scripts/tasks.sh run arm64 runtime --ip imx8mp-var-dart --dir /usr/bin --port 8000 --user root
```

Then open `http://<target-ip>:<PORT>` in your browser (including the VS Code browser) to immediately verify changes.

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
camflow -p -n 200 "v4l2src(device=/dev/video0)-CAM0>tcpsink(ip=127.0.0.1,port=9000)"
camflow -s -p -n 200 "v4l2src(device=/dev/video0)-CAM0>tcpsink(ip=127.0.0.1,port=9000)"
```

If no `-G`/`--graph` option is provided, a pipeline string argument is required.

Use `-n MAX_FRAMES` to stop the runtime after a fixed number of processed frames (`0` keeps running).

Use `-p` or `--profile` to enable per-node execution profiling. At the end of the run, the runtime prints a report with call count, failures, and min/avg/max/total execution times for each node plus a `TOTAL` summary across all nodes. Timing values are printed with 3 decimal places in milliseconds.

Use `-s` or `--simple-pipeline` to force linear execution with `PipelineSimple`. Graph edges are not used for scheduling in this mode, but are still available for default FrameContext scope inference.

Pipelines may capture images with different camera parameters, store them with `FileSink`, send them to network peers with `TCPSink`, and use processors to evaluate image content.
