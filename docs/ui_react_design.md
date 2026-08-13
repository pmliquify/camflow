# camflow React UI Architecture

This document describes how the current React implementation realizes the UI
design. It is an architecture reference, not a chronological decision log.
Visual rules live in `docs/ui_design.md`; component behavior lives in the
canonical panel specifications.

## 1. Technology and entry points

- React 18 with function components and hooks.
- JavaScript/JSX; no TypeScript layer.
- Vite 5 with `@vitejs/plugin-react`.
- Entry assets: `web/index.html`, `web/src/main.jsx`, `web/src/App.jsx` and
  `web/src/styles.css`.
- One global stylesheet defines tokens, panel layout and component classes.
- No external state-management or component library is used.

## 2. Component ownership

`App` is the state and integration owner. Its main rendered tree is:

```text
App
|- GlobalHeader
|- NodeEditorPanel
|  `- RuntimeLane[]
|     |- NodeCard[] + EdgeLayer
|     |- MediaGraphView
|     `- RuntimeLogConsole
|- FrameViewerPanel
|  `- FrameViewport + metadata/control overlays
|- ParameterPanel
|- ContextMenu
`- runtime creation Dialog
```

Shared primitives under `web/src/components` provide buttons, labels, inputs,
scroll areas, context menus, inline name editing and reset controls.

## 3. State model

### 3.1 Runtime and graph state

- Runtime status/version, remote statuses and graph discovery originate from
  REST/WebSocket integration in `App`.
- `pipelineGraph` is combined with local draft runtimes and browser-local layout,
  display-name and port-visibility state to derive `editorGraph`.
- Auto marker ids prefixed with `__auto__` remain implementation details and are
  excluded from user-facing runtime/node models.
- Graph mutations use runtime API functions and enforce stopped-pipeline
  semantics where required.

### 3.2 Selection state

- Node selection drives frame subscription and Parameter Panel content.
- Runtime selection provides fallback panel context and runtime actions.
- Media element selection temporarily switches the Parameter Panel to a
  read-only entity/pad/link inspector.
- Node selection persists and falls back to the first current pipeline node when
  a saved id is no longer available.

### 3.3 Viewport and gesture state

- Outer editor zoom/pan is owned by `App`.
- Runtime viewport state is keyed by runtime id and passed into each
  `RuntimeLane`.
- Viewer zoom/pan is owned by `App`; the canvas render transform combines fitted
  base placement with user zoom/pan.
- Gesture refs retain synchronous button/movement state while React state drives
  rendering.
- Editor, runtime and viewer pan use an 8px threshold and exact mouse-button
  masks. Cleanup occurs on release/blur and defensively on lost button state.

## 4. Data transport

### 4.1 REST

The UI uses runtime endpoints for status/version, pipeline graph, node catalog,
node parameters, graph mutation and media-device/topology discovery. API details
remain canonical in `docs/rest_api.md`.

### 4.2 WebSockets

- `/ws/frame` multiplexes frame-context JSON and matching binary image packets.
- The client sends `subscribe` commands when the selected node or image key
  changes. Binary processing is serialized and sequence-checked.
- `/ws/logs` streams unfiltered runtime/node/API/kernel records per runtime.
- Sockets are closed when their owning runtime/view state no longer requires
  them and reconnect through the application lifecycle.

## 5. Browser persistence

All storage access is guarded; unavailable or malformed storage falls back to
safe defaults.

| Key | Persisted value |
| --- | --- |
| `camflow:view-mode` | `viewer` or `editor` |
| `camflow:selected-node-id` | selected node id |
| `camflow:runtime-layouts` | runtime rectangles |
| `camflow:node-layouts` | node graph coordinates |
| `camflow:runtime-names` | runtime display names |
| `camflow:node-names` | node display/id names |
| `camflow:node-port-visibility` | visible input/output names per node |
| `camflow:frame-viewer-settings:v1` | per-node debayer state |
| `camflow:param-filter:v1` | parameter search/visibility state |
| `camflow:runtime-log-prefs:<id>` | log filters, debug mode, font and height |
| `camflow:runtime-log-default-font-size` | default for new runtime consoles |
| `camflow:runtime-media-prefs:<id>` | media device and linked-only mode |

Editor/runtime/viewer pan and zoom, panel split ratio and media selection are
session-only React state.

## 6. Layout implementation

- `App` applies `mode-editor` or `mode-viewer` to the main CSS grid.
- The desktop split ratio defaults to 0.68 and is clamped to 0.42–0.82.
- At `max-width: 980px`, CSS stacks panels and hides the splitter; the splitter
  mouse handler also refuses to start at that width.
- Panels remain mounted through the common render tree, preserving relevant
  component state as mode changes.

## 7. Editing and event boundaries

- Runtime headers own left-button window drag; controls opt out by stopping
  mouse-down propagation.
- Runtime canvases own their local wheel/pan and free-space context menu.
- The outer editor excludes runtime canvases and logs from outer navigation.
- Nodes own left-button selection/drag and port interaction, but intentionally
  have no node-surface context menu.
- Context menus open on right-button release after checking drag state. Opening
  a menu ends the corresponding pan state so later pointer movement cannot move
  a canvas.
- Edge hit paths provide a wider invisible interaction stroke than the visible
  line and own edge deletion.

## 8. Frame rendering pipeline

- Frame metadata determines canvas dimensions, aspect ratio, format, stride,
  bit shift and sequence association.
- Supported raw/Bayer, monochrome, RGB/BGR and YUYV formats are converted in the
  browser.
- A render cache reuses RGBA only when all render-affecting metadata and debayer
  state match.
- Capture and render FPS are smoothed separately and exposed in the overlay.
- Debayer preference follows a renamed node id during local state migration.

## 9. Parameter update pipeline

- Schema selects the concrete row control and runtime editability.
- Updates are type-normalized and compared with the committed-value cache before
  transmission.
- Integer sliders use a 200ms delayed send; text/numeric inputs commit on Enter
  or blur as specified.
- A full reload occurs only on selection, explicit reload or a successful update
  whose schema marks `hasSideEffects`.

## 10. Build and development configuration

`web/vite.config.js` supports:

- `CAMFLOW_WEB_OUT_DIR` (default `dist`),
- `CAMFLOW_WEB_DEV_PORT` (default `8081`),
- `CAMFLOW_WEB_API_TARGET` (default `http://127.0.0.1:8000`),
- `VITE_CAMFLOW_WS_TARGET` as an explicit browser WebSocket target.

The Vite dev server proxies `/api` and `/ws`, converts the API target to the
matching WebSocket scheme, and returns JSON `503 runtime-unreachable` or
`502 proxy-error` responses for proxy failures. Production output uses stable
`assets/app.js` and `assets/app.css` names for embedding.

## 11. Canonical specifications

- Global visual/layout design: `docs/ui_design.md`
- Header: `docs/ui_header_spec.md`
- Editor, runtimes, nodes, edges and logs: `docs/ui_editor_panel_spec.md`
- Media graph: `docs/ui_media_graph_spec.md`
- Viewer: `docs/ui_viewer_panel_spec.md`
- Parameter Panel: `docs/ui_parameter_panel_spec.md`
- Web service and launch path: `docs/web_ui.md`
