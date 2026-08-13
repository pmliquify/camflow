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
- Panning in editor, runtime and viewer canvases occurs only while the initiating middle or right mouse button remains pressed and stops immediately on release.
- The editor reset action centers all runtime windows at 100% when they fit, or fits and centers the complete runtime-window group when it is larger than the viewport.
- Each runtime canvas has an independent wheel zoom, middle/right drag pan, and reset action.
- Runtime viewport transforms apply equally to the node graph and media graph view.
- Wheel and pan input inside a runtime canvas affects only that runtime. Input on a runtime header remains assigned to the editor viewport.
- Runtime log consoles are excluded from both runtime and editor viewport navigation.
- A runtime reset action always centers all current content, including internal links. It uses 100% zoom when everything fits without shrinking; otherwise it selects the largest smaller zoom that fits everything with viewport padding.
- Runtime wheel zoom preserves the graph coordinate under the cursor across outer editor scaling. The media graph uses the same pan/zoom navigation without scrollbars.

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
- The runtime node canvas uses an unbounded graph coordinate space; nodes are not clamped to the current runtime viewport dimensions.
- Nodes may overlap.
- Node selection updates selected node state used by Parameter Panel.
- A node has a centered name header and separate input and output halves below it.
- Source input halves and sink output halves are disabled; processors enable both halves.
- Clicking either enabled port half opens a menu containing its currently hidden ports; selecting one makes it visible.
- If that side has no hidden ports, clicking it does not open a menu.
- Right-clicking a node surface does not open a context menu.
- Port visibility is stored per node and a port is shown only once even when multiple edges use it.
- Node height grows with the maximum number of visible input or output ports and initially reserves one port row.
- Double-clicking the node name opens the rename interaction. The UI sends only the requested id to the node rename endpoint.
- After the runtime accepts the id, the UI replaces all local node and edge references without reloading the graph or node parameters.

## 6. Edge interaction

- Dragging from a visible output port to a visible input port commits an edge with explicit source and target port names.
- A drop is accepted only when the selected output and input data types are compatible; no target menu is opened.
- Existing edge deletion is available via edge context interaction.
- Cross-runtime edges are supported and rendered with directional markers.
- Internal, draft and cross-runtime links are not clipped by their SVG canvas dimensions; only the owning editor viewport clips final presentation.

## 7. Context menu model

- The editor background context menu supports adding runtimes/nodes.
- A runtime context menu opens only on free canvas space inside that runtime, not on its header, nodes, ports, controls or log console.
- Editor and runtime context menus open on right-button release only when no right-button drag occurred. Pointer movement below the drag threshold does not pan the canvas.

## 8. Stability and cancel rules

- Drag, resize, and edge-draft interactions are cancelled on pointer release.
- Window blur and pointer leave cancel active drag-like states.
- Interactive controls inside draggable regions must not accidentally start drag.

## 9. Runtime integration

- Graph persistence goes through runtime REST endpoints.
- Mutating graph operations ensure stopped pipeline semantics before commit.
- Editor graph is rebuilt from pipeline plus local draft runtime state.