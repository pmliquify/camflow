# camflow Web UI Service

The UI mode starts a built-in HTTP service for a fast camera bring-up workflow.

## Purpose

Starting `camflow` without a pipeline or `-G` or `--graph` enters the special UI mode for embedded development and first customer checks.
It auto-creates a source-only pipeline with `v4l2src0` and exposes:

- live image preview in browser
- runtime parameter browser for `v4l2src` node parameters
- live frame-context execution updates over websocket
- live runtime / node / REST API / kernel logs in the runtime window console
- compact per-parameter tooltips that include description, datatype and source device

No external GUI framework is required.

The editor is supergraph-oriented:

- all runtimes are shown simultaneously as runtime lanes in one canvas
- nodes are placed inside their runtime lane
- edges may connect nodes across different runtimes

## Start

```bash
camflow --port 8080 --device /dev/video3 --subdevices /dev/v4l-subdev0,/dev/v4l-subdev3
```

Open:

```text
http://<target-ip>:8080
```

## CLI options

- `--port [PORT]`: set the web UI port, defaulting to `8080`
- `--device PATH`: initial V4L2 device, defaulting to `/dev/video3`
- `--subdevices LIST`: comma-separated initial V4L2 subdevices, defaulting to `/dev/v4l-subdev3`

UI mode starts with a stopped pipeline so `device` and `subdevices` can be selected in the UI before capture starts.

## HTTP endpoints

- `GET /`: web UI page
- `GET /api/nodes`: available node types grouped by kind
- `GET /api/runtime`: current runtime state (`running` or `stopped`)
- `GET /api/runtime/version`: runtime version metadata (`v<app-version>`, git, build timestamp, OpenCV version)
- `PUT /api/runtime`: set runtime desired state (`running` or `stopped`)
- `GET /api/nodes/v4l2src0/parameters`: current live `v4l2src` parameter schema + values
- `PUT /api/nodes/v4l2src0/parameters/{name}`: update one runtime-writable parameter

## WebSocket endpoint

- `GET /ws/frame`: websocket stream carrying both framecontext metadata (JSON text) and matching binary image packets
- `GET /ws/logs`: unfiltered websocket stream carrying runtime, node, REST API and kernel log records for the selected runtime window; console `-L` filtering does not affect it

Each message contains:

- `nodeId`
- context `keys`
- `hasImage` flag
- image metadata (`width`, `height`, `sequence`, `timestampNs`) when available

## Image path

Captured frames are transferred as raw binary packets over `GET /ws/frame` and are paired with context messages on the same stream.
All pixel-format conversion is done in the browser.

The frame overlay in the top-left corner shows:

- `seq: #<seq> | ts: <ts> ms`
- `capture fps: <fps> | render fps: <fps>`

Top-right hover tools are format-aware:

- optional `rgb` debayer toggle for Bayer formats

The frame packet's `bitShift` metadata is applied automatically before RAW conversion
or debayering.

Image interaction:

- mouse wheel: zoom
- right mouse drag: pan

Node editor interaction:

- mouse wheel: zoom in/out of the supergraph canvas
- middle mouse drag: pan the supergraph canvas
- runtime overlays and cross-runtime edges scale with zoom

The runtime badge next to `camflow` shows `running`/`stopped` and switches to `down` (red background) when the service is offline.

Each runtime window includes a collapsible, resizable log console at the bottom. The console can filter messages by source (`kernel`, `runtime`, `node`, `api`) and can apply a kernel regex filter while editing.

## Parameter model

The parameter panel specification was moved to a dedicated document:

- `docs/ui_parameter_panel_spec.md`

This includes:

- selection persistence and default selection behavior
- exact full-reload trigger rules
- side-effect schema handling (`hasSideEffects`)
- per-type commit behavior and update throttling
- no-op suppression (no runtime update for unchanged values)

## Scope

UI mode now runs through the normal pipeline implementation so scheduler and runtime behavior match regular `camflow` execution.

## Multi-runtime orchestration

UI component specifications are separated into dedicated documents:

- Header: `docs/ui_header_spec.md`
- Editor Panel: `docs/ui_editor_panel_spec.md`
- Viewer Panel: `docs/ui_viewer_panel_spec.md`
