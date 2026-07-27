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
- Runtime lane bottom area includes an embedded, resizable log console for runtime, REST API and kernel messages.

## 5. Node interaction

- Nodes are draggable inside their runtime lane.
- Nodes cannot overlap.
- Node selection updates selected node state used by Parameter Panel.

## 6. Edge interaction

- User can create edges by source-to-target node interaction.
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