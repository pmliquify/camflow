# Development Guide

## Code style

- Do not use namespaces.
- Member variables use `m_variable` naming.
- `.hpp` and `.cpp` files stay in the same functional directory.
- Use logger macros instead of `std::cout` or `std::cerr` outside help output.
- Use `typedef` aliases for common smart pointer types.
- Declare local variables as late and as locally as possible, ideally immediately
	before their first use.
- Format C++ code with clang-format 15.

## Context contract model

- `NodeSchema` is the node contract and consists of `parameters`, `inputs` and `outputs`.
- Parameters describe node-owned settings and are stored in `Node`.
- Inputs describe bindable dependencies and their data types.
- Outputs describe produced data keys (for example `image`).
- Input bindings are configured in pipeline expressions as `<input>=<nodeId>.<output>`.
- Inputs with `allowMultipleBindings=true` may bind multiple outputs.
- Runtime parameter updates are handled generically in `Node`. Derived nodes react
	through a change callback and should only apply direct side effects there.
- Nodes read configured values through `Node::parameter(...)` and the typed helpers
	`parameterBool(...)`, `parameterInt(...)`, `parameterDouble(...)`,
	`parameterString(...)`.
- In `process`, read node-owned parameters directly from `Node` instead of
	reading `<nodeId>.<parameter>` from `FrameContext`.
- In `process`, resolve node inputs via the binding helpers in `Node` and read
	the bound values from `FrameContext`.
- Do not implement node-local parameter conversion helpers. Keep conversion logic
	central in `Node` so all nodes behave consistently.
- Avoid storing duplicated member fields for parameter values in node classes.
	Read active settings from the generic node parameter store.

## Zero-copy implementation requirement

- Image payloads and context values must be transported without deep copies whenever backend memory lifetime allows it.
- Context propagation must share value ownership across nodes, branches and merges.
- Values may only be replaced when a key is explicitly overwritten by a node.
- New node implementations must preserve this behavior and use move semantics when writing large values into `FrameContext`.

## Adding runtime nodes

Runtime nodes are stored below `src/nodes`.

- Sources: `src/nodes/sources`
- Processors: `src/nodes/processors`
- Sinks: `src/nodes/sinks`

Every node must provide a parameter schema. The REST API and help system derive their output from this schema.

## OpenCV

OpenCV 4.12 is required and linked statically.

Build only needed modules (`core`, `imgproc`, `imgcodecs`) to keep size and build time low.
The project build uses a speed-oriented Release profile with intrinsics, fast-math and
link-time optimization. x86_64 enables IPP and runtime SIMD dispatch through AVX-512;
ARM64 uses NEON as its baseline instruction set.

OpenCV is built into the project-local tree:

- install prefix: `opencv/4.12.0/linux-x86_64` or `opencv/4.12.0/linux-arm64`
- build directory: `opencv/4.12.0/build-linux-x86_64` or `opencv/4.12.0/build-linux-arm64`

The `build-linux-*` directories are temporary build trees and can be deleted after a successful install.
The `linux-*` directories contain the reusable installed OpenCV artifacts used by runtime builds.
Each install stores its optimization profile in `.camflow-build-profile`; changing the
profile in the installer causes the package to be rebuilt automatically.

## GStreamer policy

`NVArgusSource` may use GStreamer. It is compiled and registered only when GStreamer is available.

## Building the runtime

```bash
cmake -B build-runtime -DBUILD_RUNTIME=ON
cmake --build build-runtime --parallel
```

## Task automation script

Use `scripts/tasks.sh` as the unified entry point for build, deploy, run, dependency setup, formatting, and documentation workflows.

Behavior:

- If `scripts/tasks.cfg` does not exist, `tasks.sh` creates it.
- In an interactive shell, `tasks.sh` asks for all default values and stores them in `scripts/tasks.cfg`.
- In a non-interactive context, `tasks.sh` creates `scripts/tasks.cfg` with default values.

Common usage:

```bash
./scripts/tasks.sh --help
./scripts/tasks.sh deps --opencv 4.12.0
./scripts/tasks.sh format --clang-format clang-format-15
./scripts/tasks.sh docs --clean --install-deps
./scripts/tasks.sh build dev ui
./scripts/tasks.sh build dev runtime
./scripts/tasks.sh build arm64 runtime
./scripts/tasks.sh build deploy run arm64 runtime --ip imx8mp-var-dart --dir /usr/bin --user root --port 8000
```

The script supports command groups (`build`, `deploy`, `run`, `deps`, `format`, `docs`), platform selectors (`dev`, `arm64`), and product selectors (`runtime`, `ui`).

## VS Code tasks and scripts/tasks.cfg

The workspace defines exactly four development tasks in `.vscode/tasks.json`:

1. `Camflow: Build and restart runtime (local)`
2. `Camflow: Run UI dev server (local)`
3. `Camflow: Build and restart runtime (remote)`
4. `Camflow: Run UI dev server (remote)`

All task values are sourced from `scripts/tasks.cfg`.

Important keys:

- `BUILD_TYPE`: CMake build type (default `Release`)
- `TARGET`: remote runtime host (`pollux` or `imx8mp-var-dart`)
- `PORT`: runtime UI/REST port (default `8000`)
- `UI_DEV_PORT`: local Vite dev server port (default `8081`)
- `UI_API_PORT`: local UI proxy target port (default `8000`)

To switch remote targets during development, edit only this field:

```properties
TARGET=pollux
# or
TARGET=imx8mp-var-dart
```

Then rerun the two remote tasks. No task file edits are required.

## Runtime CLI options

- `-G`, `--graph`: Load a graph from JSON.
- `-n`: Process at most N frames (`0` = unlimited).
- `-p`, `--profile`: Enable the node profiling report on shutdown. The report is
	categorized as `application` console output.
- `-s`, `--simple-pipeline`: Use linear `PipelineSimple` execution.
- `-v`, `--verbose`: Print all WebServer requests.
- `-L`, `--log-source LIST`: Select console log sources. Available sources are
	`application`, `runtime`, `node`, `api` and `kernel`; the default is
	`application,node`.
- `--debug`: Enable detailed logger output.
- `--rest-api`: Start the REST API in pipeline mode.
- `--port [PORT]`: Set the built-in web UI or REST API port, defaulting to `8000` when no port is given.
- `--device PATH`: Initial UI-mode V4L2 device, defaulting to `/dev/video3`.
- `--subdevices LIST`: Comma-separated initial UI-mode V4L2 subdevices, defaulting to `/dev/v4l-subdev3`.

`--log-source` accepts comma-separated values and may be repeated. Repeated
options add sources; `all` and `none` reset the selection, and a `-` prefix
removes a source:

```bash
camflow -L node,api PIPELINE
camflow -L node -L kernel PIPELINE
camflow -L all,-api,-kernel PIPELINE
camflow -L none PIPELINE
```

This filter applies only to process console output. Logger history and the
`/ws/logs` stream always contain all `application`, `runtime`, `node`, `api` and `kernel`
records. Each web UI runtime console filters that complete stream locally.

UI runtime details:

- UI mode starts with a stopped pipeline so `device` and `subdevices` can be set in the UI before capture starts.
- UI mode runs as a source-only graph (`v4l2src0`), frame streaming is provided by runtime FrameContext observers.
- Websocket events are emitted for the configured UI source node (`v4l2src0` in UI mode).
- `v4l2src.device` and `v4l2src.subdevices` are live `option` parameters populated from `/dev` entries.
- frame-context execution events and matching binary images are streamed over websocket at `/ws/frame`.
- raw image frames are streamed on demand over websocket at `/ws/frame`.
