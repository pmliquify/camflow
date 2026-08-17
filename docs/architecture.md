# Architecture

CamFlow v0.5.0 is a runtime-only Linux project.

## Runtime

The runtime executable is `camflow`.

Supported target architectures:

- Linux x86_64
- Linux arm64

The runtime owns:

- V4L2 camera capture
- image processing pipeline execution
- REST API for runtime control
- built-in web UI mode

## Directory structure

```text
camflow/
  src/
    runtime/
  docs/
  tests/
  scripts/
```

## Build options

The top-level CMake project exposes:

- `BUILD_RUNTIME`: build the Linux runtime (ON)
- `ENABLE_GSTREAMER`: enable GStreamer-dependent runtime nodes

OpenCV 4.12 is required and linked statically into the runtime binary.

## Data flow

A pipeline graph is composed of source, processor and sink nodes.

Example:

```text
v4l2src(device=/dev/video0) -> debayer -> tcpsink(ip=192.168.1.20,port=9000)
```

The built-in UI mode creates a single `v4l2src` node and serves a browser UI for live image preview and runtime parameter updates.

## Logging

Log records are categorized as `application`, `runtime`, `node`, `api` or `kernel`. The logger
always stores every record in its history and publishes every record to listeners,
including the `/ws/logs` web UI transport.

Messages emitted directly by node implementations are `node`; pipeline lifecycle
and runtime transport messages are `runtime`; REST API request/response messages
are `api`; kernel records are `kernel`. Every other message is `application`.

Console output is a separate sink with its own source mask. The CLI option
`-L`/`--log-source` configures only that sink and defaults to `application,node`; the web UI
always receives the complete stream and applies its source filters locally.
