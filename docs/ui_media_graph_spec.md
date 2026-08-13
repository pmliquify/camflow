# camflow UI Specification: Media Graph

This document is the canonical specification for the media-controller topology
view rendered by `MediaGraphView` inside a runtime window.

## 1. Scope and activation

- The media graph is a runtime-local alternative to the node graph.
- The runtime-header media icon toggles the mode.
- Activating the mode loads available `/dev/mediaX` devices and the graph for
  the selected device from that runtime.
- Leaving the mode clears the current media-element selection.
- Loading, API errors, no-device, empty-graph and no-connected-link states are
  rendered inside the runtime canvas.

## 2. Header controls

Media mode adds the following controls to the owning runtime header:

- device selector populated from the runtime,
- `linked` toggle that limits the view to enabled connections,
- reload action for the selected device,
- media-mode toggle used to return to the node graph.

Changing devices reloads immediately. Device and connected-only choices persist
per runtime under `camflow:runtime-media-prefs:<runtimeId>`.

## 3. Navigation and viewport

- Media entities and links use the same independent zoom, pan and reset state as
  the runtime node graph.
- Wheel zoom range is `0.01x` to `2.25x` and remains pointer-centered.
- Middle/right drag pans only while the initiating button remains pressed and
  after the 8px threshold has been crossed.
- Reset centers the complete laid-out topology at 100% when it fits, otherwise
  at the largest smaller fitting zoom with runtime padding.
- The fixed media viewport clips final presentation and has no scrollbars; only
  `.media-graph-canvas` receives the transform.

## 4. Graph filtering and layout

- The full graph contains entities, pads and links returned by the runtime API.
- Connected-only mode keeps enabled links and only their participating entities
  and pads.
- Enabled links define the initial topological layers.
- Cyclic components receive deterministic finite layers.
- Entities within layers are ordered by alternating barycentric sweeps followed
  by adjacent-swap improvement to reduce crossings.
- Layout constants are implementation-level CSS-pixel values: 150px entity
  width, 64px column gap, 30px row gap and 28px outer margin.
- The graph canvas is at least `480x220` and expands to contain all entities.

## 5. Entity, pad and link rendering

- Entity cards show numeric id, name, device node or function, and their pads.
- Pads identify source/sink direction and expose format/dimension details in a
  tooltip when available.
- Links use directional Bezier paths and arrowheads.
- Enabled, disabled, immutable and dynamic link flags have distinct visual
  treatments. Immutable links use a double-line treatment; dynamic links add a
  dotted overlay.
- Selected entities, pads and links receive a dedicated highlight without
  changing graph data.

## 6. Selection and inspection

- Clicking an entity, pad or link creates a single media-element selection.
- The shared Parameter Panel switches to a read-only inspector for that element.
- Entity inspection includes function, pad count and entity flags.
- Pad inspection includes id and pad flags.
- Link inspection includes source/sink relation and link flags.
- Flag controls show the numeric hexadecimal value and named set/unset flags;
  they never write to the runtime.
- Changing device, toggling connected-only mode or leaving media mode clears the
  selection so stale graph objects are not inspected.

## 7. Non-goals

- The current media graph does not edit entities, pads, formats, routes or flags.
- It does not use browser scrollbars for navigation.
- Media selection is inspection state, not node selection and is not persisted.
