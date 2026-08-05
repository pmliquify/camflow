# camflow UI Specification: Editor Panel

This document specifies the graph editor panel (`NodeEditorPanel`) behavior.

## 1. Scope

- Component: `NodeEditorPanel` with runtime lanes and edges.
- Provides supergraph editing across local and remote runtimes.

## 2. Visual model

- Editor contains runtime windows (lanes), each holding nodes.
- Cross-runtime edges are rendered on a separate absolute edge layer.
- Internal auto-generated node marker (`__auto__`) is never user-facing.

## 3. Navigation

- Mouse wheel zooms editor canvas within configured limits.
- Middle/right drag pans the editor viewport.
- Reset action restores default transform.

## 4. Runtime lane interaction

- Runtime lane windows are draggable.
- Runtime lane windows are resizable.
- Lanes must not overlap after move/resize.
- Runtime header includes:
  - runtime display label
  - runtime status badge
  - runtime start/stop action
  - inline runtime address editing support
  - log console show/hide action
- Runtime lane bottom area includes an embedded, resizable log console for runtime, node, REST API and kernel messages.

## 5. Node interaction

- Nodes are draggable inside their runtime lane.
- Nodes may overlap.
- Node selection updates selected node state used by Parameter Panel.
- A node has a centered name header and separate input and output halves below it.
- Source input halves and sink output halves are disabled; processors enable both halves.
- Clicking either enabled port half opens a menu containing its currently hidden ports; selecting one makes it visible.
- If that side has no hidden ports, clicking it does not open a menu.
- Right-clicking a visible input or output port removes every edge connected through that port before hiding it.
- Port visibility is stored per node and a port is shown only once even when multiple edges use it.
- Node height grows with the maximum number of visible input or output ports and initially reserves one port row.
- Double-clicking the node name opens the rename interaction. The UI sends only the requested id to the node rename endpoint.
- After the runtime accepts the id, the UI replaces all local node and edge references without reloading the graph or node parameters.

## 6. Edge interaction

- Dragging from a visible output port to a visible input port commits an edge with explicit source and target port names.
- A drop is accepted only when the selected output and input data types are compatible; no target menu is opened.
- Existing edge deletion is available via edge context interaction.
- Cross-runtime edges are supported and rendered with directional markers.

## 7. Context menu model

- Editor background context menu supports adding runtimes/nodes.
- Runtime/node context menus support rename/delete operations where allowed.

## 8. Stability and cancel rules

- Drag, resize, and edge-draft interactions are cancelled on pointer release.
- Window blur and pointer leave cancel active drag-like states.
- Interactive controls inside draggable regions must not accidentally start drag.

## 9. Runtime integration

- Graph persistence goes through runtime REST endpoints.
- Mutating graph operations ensure stopped pipeline semantics before commit.
- Editor graph is rebuilt from pipeline plus local draft runtime state.