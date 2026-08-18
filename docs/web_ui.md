# camflow Web UI Service

The built-in web UI combines runtime control, supergraph editing, raw-frame
inspection, parameter editing, media-topology inspection and per-runtime logs in
one browser workspace.

## 1. UI mode

Starting `camflow` without a pipeline or `-G`/`--graph` enters the embedded UI
workflow. It creates a source-oriented initial pipeline and serves the UI and
runtime API from the configured HTTP port.

```bash
camflow --port 8080 --device /dev/video3 --subdevices /dev/v4l-subdev0,/dev/v4l-subdev3
```

Open `http://<target-ip>:8080` (HTTP, not HTTPS). The embedded server does not
terminate TLS; use a reverse proxy with a certificate if HTTPS is required.

Relevant options:

- `--port [PORT]`: HTTP/UI port, default `8080`,
- `--device PATH`: initial V4L2 device, default `/dev/video3`,
- `--subdevices LIST`: comma-separated initial subdevices.

UI mode initially keeps the pipeline stopped so device/subdevice parameters can
be configured before capture.

## 2. Workspace features

- Global runtime status/version, graph status, Start/Stop and mode switch.
- Supergraph editor showing local and remote runtime windows simultaneously.
- Independent editor and per-runtime zoom/pan/reset navigation.
- Runtime/node layout, port visibility, edge creation and graph persistence.
- Runtime-local media-controller entity/pad/link topology inspection.
- Per-runtime, resizable log consoles with source/regex filtering.
- Live raw-frame viewer with browser-side conversion, debayer, metadata,
  pointer-centered zoom and pan.
- Schema-driven Parameter Panel with runtime-aware editability and persistent
  visibility filtering.
- Responsive desktop split layout and stacked layout at 980px or below.

Detailed behavior is canonical in:

- visual system/layout: `docs/ui_design.md`,
- React architecture/state: `docs/ui_react_design.md`,
- header: `docs/ui_header_spec.md`,
- editor/runtimes/nodes/edges/logs: `docs/ui_editor_panel_spec.md`,
- media graph: `docs/ui_media_graph_spec.md`,
- viewer: `docs/ui_viewer_panel_spec.md`,
- parameters/media inspection: `docs/ui_parameter_panel_spec.md`.

## 3. Runtime transport

### 3.1 HTTP

The UI consumes the runtime API under `/api`, including:

- runtime state and version,
- pipeline graph and node catalog,
- node parameter schema/values and updates,
- node/edge/runtime graph mutations,
- media-device discovery and media graph data.

The complete endpoint contract is maintained in `docs/rest_api.md`.

### 3.2 WebSockets

- `/ws/frame` carries frame-context JSON and matching binary image packets.
- `/ws/logs` carries unfiltered application, runtime, node, REST API and kernel records.

The frame client sends `subscribe` commands with selected node/image context and
sequence-checks matching binary packets. The log stream is independent of CLI
`-L` source filtering; filtering occurs per runtime console in the browser.

## 4. Frame path

- Raw packets are converted client-side; the UI does not require a server JPEG
  path.
- Context metadata supplies dimensions, format, stride, sequence, timestamp and
  `bitShift`.
- Multiple context images are supported through image keys; the selected key is
  retained when available, otherwise the first advertised image is used.
- Bayer debayer is format-aware and user-controlled; bit shift is automatic.
- Converted RGBA is cached for identical render conditions.
- Viewer streaming is active only while the selected runtime is running.

## 5. Runtime and graph behavior

- All runtimes share one outer editor coordinate space.
- Runtime windows cannot overlap; nodes use unbounded local coordinates and may
  overlap.
- Cross-runtime edges are rendered in a separate editor SVG layer.
- Mutating graph operations stop the pipeline first when required.
- The local runtime cannot be removed from the runtime context menu.
- A new runtime dialog accepts display name and IP/host. The display name can be
  edited later in the header; the IP/host is not inline-editable there.

## 6. Local browser state

The UI persists presentation preferences in `localStorage`:

- view mode and selected node,
- runtime/node layout and display names,
- visible node ports,
- per-node debayer setting,
- parameter visibility filter,
- per-runtime log settings and global default log font,
- per-runtime media device and connected-only preference.

Zoom/pan transforms, splitter ratio and current media selection are session-only.
All storage reads/writes degrade safely when browser storage is unavailable.

## 7. Web development

From `web/`:

```bash
npm run dev
npm run build
npm run preview
```

Vite configuration variables:

| Variable | Default | Purpose |
| --- | --- | --- |
| `CAMFLOW_WEB_OUT_DIR` | `dist` | production output directory |
| `CAMFLOW_WEB_DEV_PORT` | `8081` | strict Vite development port |
| `CAMFLOW_WEB_API_TARGET` | `http://127.0.0.1:8000` | `/api` and `/ws` proxy target |
| `VITE_CAMFLOW_WS_TARGET` | empty | explicit browser WebSocket origin override |

The development server proxies `/api` and `/ws`. Expected refused/reset runtime
connections return JSON `503 runtime-unreachable`; other proxy failures return
`502 proxy-error`.

Production output disables source maps and CSS splitting and uses stable embedded
asset names:

- `assets/app.js`,
- `assets/app.css`.

Imported graphics are inlined into these bundles so the runtime binary does not
depend on separately served image files.

`window.CAMFLOW_SOURCE_NODE_ID` in the generated page supplies the initial source
node id used by the frame subscription, with `v4l2src0` as the runtime fallback.

## 8. Generated documentation boundary

Files below `docs/api/` are generated by Doxygen and are not edited as UI design
sources. The Markdown files listed above are the maintained UI documentation.
