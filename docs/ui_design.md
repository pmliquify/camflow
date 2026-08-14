# camflow Web UI Design

This document defines the current visual system, global layout and interaction
language of the built-in camflow web UI. Component behavior is canonical in the
linked panel specifications.

## 1. Product direction

The UI is a compact technical workspace for camera bring-up, graph editing,
live image inspection and runtime diagnostics.

- Dark, instrument-like presentation with cyan/blue interaction accents.
- Dense controls and overlays favor repeated engineering workflows over
  marketing composition.
- Runtime, graph, image and parameter state remain visible without navigation
  to separate pages.
- The UI has no separate status strip; runtime, discovery and error status are
  integrated into the global header.

## 2. Visual system

### 2.1 Typography

- Primary family: `Rajdhani` for labels, controls, headers and graph text.
- Monospace family: `Roboto Mono` with platform monospace fallbacks for content
  that benefits from fixed-width alignment.
- Base UI text is 13px; compact controls generally use 10–13px.
- Product name is 27px and graph/runtime labels remain compact.

### 2.2 Core tokens

The implementation exposes these CSS variables:

- background: `--bg: #080f1b`, `--bg2: #111b2e`,
- panel: `--panel: rgba(17, 30, 50, 0.96)`,
- border: `--line: #223a5f`,
- text: `--text: #e8f1ff`,
- muted text: `--muted: #95a9c8`,
- primary accent: `--accent: #02d7ff`,
- secondary accent: `--accent2: #3ae0a7`.

The page background combines two restrained radial fields with a dark linear
gradient. Panels use layered dark fills, 1px borders, 14px outer radii and a
soft depth shadow. Adjacent panes remove shared-side radii/borders so they read
as one workspace rather than separate cards.

### 2.3 Semantic status

- `running`: green success treatment,
- `stopped`: blue information treatment,
- `down`: red danger treatment,
- operation errors: pale red header-status text.

Status is communicated by both text and color.

## 3. Application shell

- The application fills the viewport and prevents page-level scrolling.
- A sticky full-width header remains above the workspace.
- The workspace uses a two-column grid above 980px and a stacked grid at or
  below 980px.
- The draggable vertical splitter defaults to 68% left width, is clamped to
  42–82%, maintains a 320px minimum right column and is hidden on narrow
  layouts. Split ratio is session state, not browser-persisted state.

### 3.1 Editor mode

Desktop grid:

- left column: Node Editor spanning both rows,
- upper right: Frame Viewer,
- lower right: Parameter Panel.

At 980px or below the order becomes Editor, Viewer, Parameter Panel.

### 3.2 Viewer mode

Desktop grid:

- left column: Frame Viewer,
- right column: Parameter Panel,
- Node Editor hidden by layout mode.

At 980px or below the Viewer is stacked above the Parameter Panel.

Switching mode changes placement/visibility, not the underlying selected node,
frame, parameter or graph state. Mode persists under `camflow:view-mode`.

## 4. Header

The header combines brand/version, runtime status, graph/error status, global
Start/Stop and the `viewer`/`editor` segmented mode switch. There is no separate
status strip and no graph-edit action in the header.

Canonical behavior: `docs/ui_header_spec.md`.

## 5. Editor and runtimes

The editor presents a supergraph of non-overlapping runtime windows. Runtime
windows contain an independently navigable node graph or media graph plus an
optional diagnostic log console. Outer editor navigation and each runtime's
inner navigation are deliberately isolated.

Canonical behavior: `docs/ui_editor_panel_spec.md`.

Media topology behavior: `docs/ui_media_graph_spec.md`.

## 6. Frame Viewer

The viewer preserves the source image aspect ratio, uses a canvas for converted
pixels and overlays compact metadata and format-aware controls. The fixed frame
box clips a separately transformed canvas, allowing pointer-centered zoom and
pan without affecting surrounding layout.

Canonical behavior: `docs/ui_viewer_panel_spec.md`.

## 7. Parameter Panel

The Parameter Panel is a schema-driven editor for the selected node and a
read-only inspector for selected media entities, pads and links. It uses compact
type-specific controls, grouped rows, persistent visibility filtering and
runtime-aware editability.

Canonical behavior: `docs/ui_parameter_panel_spec.md`.

## 8. Interaction language

- Familiar icon-only actions use tooltips and accessible labels.
- Binary choices use active-state buttons or checkboxes; modes use segmented
  controls; numeric ranges use slider plus input.
- Middle/right drag pans editor, runtime and viewer canvases only while the
  initiating button remains pressed and after an 8px threshold.
- Wheel zoom is pointer-centered. Reset controls restore deterministic centered
  transforms instead of toggling between arbitrary previous states.
- Interactive controls stop drag initiation in draggable headers/surfaces.
- Context menus open only on explicitly owned free-canvas surfaces; native menus
  are suppressed where right drag or application actions own the gesture.

## 9. Accessibility

- Commands use semantic buttons and form controls.
- Icon-only buttons expose `title` and/or `aria-label` text.
- Active mode/status states are represented in text and classes, not color alone.
- The layout splitter and log resize handle expose separator roles and labels.
- Viewer and parameter overlays maintain high contrast over live imagery.
- Keyboard focus uses one high-contrast cyan ring across buttons, links,
  editable controls and custom focusable elements.
- Custom checkboxes and sliders expose the same ring on their visible control.
  A focused slider highlights its complete track and handle so focus remains
  apparent while stepping through parameter values with `Tab`.

## 10. Responsive and overflow rules

- Fixed-format controls have stable dimensions so active/hover states do not
  shift layout.
- Runtime and editor viewports clip final graph presentation; edge paths may
  overflow intermediate SVG layers.
- Parameter and log areas own their scrolling and prevent wheel propagation into
  parent canvas navigation.
- The Media Graph has no scrollbars and uses runtime pan/zoom exclusively.
- Narrow layouts stack panels without changing their content model.
