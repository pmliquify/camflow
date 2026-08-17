# camflow UI Specification: Editor Panel

This document is the canonical specification for `NodeEditorPanel`,
`RuntimeLane`, node/edge editing, runtime-local navigation and runtime logs.
Media graph rendering is detailed in `docs/ui_media_graph_spec.md`.

## 1. Scope

- Component: `NodeEditorPanel` with runtime lanes and edges.
- Provides supergraph editing across local and remote runtimes.

## 2. Visual model

- The editor is a supergraph containing all local and remote runtime windows.
- Each runtime owns either its node graph or its media graph plus an optional
  log console.
- Internal runtime edges, edge drafts and cross-runtime edges use separate SVG
  layers. Cross-runtime edges are positioned in editor coordinates.
- Internal auto-generated node marker (`__auto__`) is never user-facing.
- Runtime windows clip their visible contents, while edge SVG layers allow
  paths to extend beyond their own SVG bounds before final viewport clipping.

## 3. Navigation

- Plain mouse wheel zooms the editor from `0.35x` to `2.25x` and preserves the
  graph coordinate under the pointer.
- `Shift` + wheel pans horizontally. `Alt` + wheel pans vertically.
- Middle/right drag pans the complete editor viewport, including free visible
  space outside the transformed inner canvas.
- Pan starts only after an 8px drag threshold. It continues only while the
  initiating middle or right button remains pressed and stops immediately on
  release, blur or lost button state.
- The editor reset action centers all runtime windows at 100% when they fit, or fits and centers the complete runtime-window group when it is larger than the viewport.
- Each runtime canvas has an independent `0.01x` to `2.25x` wheel zoom,
  middle/right pan and reset action. Its zoom preserves the local graph point
  under the pointer even when the outer editor is scaled.
- Node graph and media graph keep independent runtime viewport transforms and
  restore their last zoom and pan when switching views.
- Runtime navigation input affects only that runtime. Runtime headers remain
  part of the outer editor navigation surface.
- Runtime log consoles are excluded from both runtime and editor viewport navigation.
- Runtime reset measures nodes and internal links, or media graph dimensions.
  It centers at 100% when content fits and otherwise chooses the largest zoom
  at or below 100% that fits with 16px padding.
- On initial UI load, the editor automatically fits and centers all runtime
  windows once when the editor first becomes visible. Each runtime node view
  and media graph independently perform the same one-time fit when their
  content first becomes visible.
- The media graph viewport has no scrollbars; navigation uses the same runtime
  transform.

## 4. Runtime lane interaction

- Runtime windows are dragged with the left button on non-interactive header
  space and resized from the bottom-right grip.
- Minimum runtime size is `190x120` CSS pixels.
- Runtime move/resize accounts for editor zoom. A candidate rectangle is
  accepted only when it respects minimum size and does not overlap another
  runtime window.
- Runtime header includes:
  - runtime icon and inline-editable display name,
  - status badge (`running`, `stopped`, `down`),
  - media graph mode toggle and its mode-specific controls,
  - log console show/hide toggle,
  - runtime `Start`/`Stop` action.
- Double-click edits the display name; the runtime IP/host is supplied by the
  creation dialog and is not inline-editable in the header.
- Header buttons and fields do not start window dragging.
- The local runtime cannot be deleted through the runtime context menu.

## 5. Node interaction

- Nodes are draggable inside their runtime lane.
- The runtime node canvas uses an unbounded graph coordinate space; nodes are not clamped to the current runtime viewport dimensions.
- Nodes may overlap.
- Selection is single-node. Clicking a node updates the Parameter Panel;
  dragging suppresses the following click selection.
- A node has a centered name header and separate input and output halves below it.
- Source input halves and sink output halves are disabled; processors enable both halves.
- Clicking either enabled port-side area opens a menu containing its currently
  hidden ports; selecting one makes it visible.
- If that side has no hidden ports, clicking it does not open a menu.
- Right-clicking a node surface does not open a context menu.
- Port visibility is stored per node in `camflow:node-port-visibility`. The
  first available input and output are initially visible, and a port is shown
  only once even when multiple edges use it.
- Node height grows with the maximum number of visible input or output ports and initially reserves one port row.
- Double-clicking the node name opens the rename interaction. The UI sends only the requested id to the node rename endpoint.
- After the runtime accepts the id, the UI replaces all local node and edge references without reloading the graph or node parameters.

## 6. Edge interaction

- Left-dragging from a visible output port displays a live Bezier draft path to
  the pointer. Releasing over a visible input port commits an edge with explicit
  source and target port names.
- A drop is accepted only when the selected output and input data types are compatible; no target menu is opened.
- Self-connections and duplicate edges are rejected.
- Right-clicking an internal or cross-runtime edge hit path immediately deletes
  that edge; it does not open an application context menu.
- Cross-runtime edges are supported and rendered with directional markers.
- Internal, draft and cross-runtime links are not clipped by their SVG canvas dimensions; only the owning editor viewport clips final presentation.

## 7. Context menu model

- Free editor background offers `New Runtime`.
- Free runtime canvas offers grouped source/processor/sink node creation and,
  for non-local runtimes, `Delete Runtime`.
- No application menu opens on runtime headers, nodes, ports, controls, logs or
  edge hit paths.
- Editor and runtime menus open on right-button release only when no right drag
  occurred. Movement below 8px does not pan; movement at or above 8px pans and
  suppresses the menu.
- Opening a menu ends the active pan gesture. Moving the pointer after release
  never pans, even while the menu remains open.

## 8. Runtime log console

- Each runtime has a collapsible log console below its graph canvas. It receives
  application, runtime, node, REST API and kernel records from `/ws/logs`.
- The top resize handle constrains height to `112px` through `320px` and also to
  the available runtime-window height. Default height is `154px`.
- Source toggles independently show/hide `application`, `runtime`, `node`, `api` and `kernel`.
- Kernel records can additionally be filtered by a case-insensitive regular
  expression. Invalid syntax is reported inline and does not apply a regex.
- `Alt+G` (`Option+G` on macOS) opens the selected runtime's console and filter,
  focuses the kernel-regex input and selects its complete value. Repeating the
  shortcut reselects the current value.
- `Escape` closes runtime-log filter mode without clearing source selections or
  the kernel regex.
- Debug-details mode restores timestamp, type, source location or kernel tag;
  normal mode strips those prefixes for compact reading.
- Font controls span `5px` through `15px`; clear removes buffered records.
- The console follows new records while already near the bottom and preserves
  manual scroll position otherwise. Selected text supports native copy and a
  clipboard fallback.
- Auto-follow reacts to the newest displayed record rather than only the record
  count, so it continues after the 300-record display limit starts replacing
  old records.
- Per-runtime preferences persist filter-panel state, source visibility, kernel
  regex, debug-details mode, font size and height under
  `camflow:runtime-log-prefs:<runtimeId>`. The latest font size is also the
  global default for newly encountered runtimes.
- The UI retains at most 300 displayed records per runtime and bounds duplicate
  tracking to 1200 keys.

## 9. Media graph mode

- The runtime header toggles between the node graph and media graph without
  changing the owning runtime window or its local viewport.
- Device selection, connected-only filtering, reload, selection and inspection
  behavior are defined in `docs/ui_media_graph_spec.md`.

## 10. Stability and cancel rules

- Drag, resize, and edge-draft interactions are cancelled on pointer release.
- Window blur and pointer leave cancel active drag-like states.
- Interactive controls inside draggable regions must not accidentally start drag.
- Navigation and drag handlers verify the actual pressed-button mask, preventing
  sticky pan after a context menu or lost mouse-up propagation.

## 11. Runtime integration and persistence

- Graph persistence goes through runtime REST endpoints.
- Mutating graph operations ensure stopped pipeline semantics before commit.
- Editor graph is rebuilt from pipeline plus local draft runtime state.
- Graph mutations stop the pipeline before committing when required by the
  runtime API.
- Browser-local presentation state uses:
  - `camflow:runtime-layouts` for runtime rectangles,
  - `camflow:node-layouts` for node coordinates,
  - `camflow:runtime-names` and `camflow:node-names` for display names,
  - `camflow:node-port-visibility` for visible ports.
- Runtime-local zoom/pan is session state and is not persisted.