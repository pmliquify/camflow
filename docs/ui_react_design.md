# camflow React UI Design Notes

This document captures all UI design requirements discussed specifically for the React-based embedded UI. It complements and details the baseline design from docs/ui_design.md.

## 1. Design Source

- Primary design baseline: docs/ui_design.md
- React implementation must follow its color, spacing, typography and interaction direction.
- This file is the React-specific execution log of all additional design decisions.

## 2. Typography and Sizing (Strict)

- Use one font family for the complete UI: Rajdhani.
- Apply Rajdhani consistently to all UI elements (text, controls, labels, graph text, header, badges).
- Target font sizes from the base design must be respected:
  - App name camflow: 26px
  - Normal controls and labels: 11px to 13px
  - Graph text: 12px
  - Status strip text: 12px

## 3. Global Edge and Header-Line Behavior

- Header and status-strip separator lines must visually run across the full browser width.
- Header/status content text must keep a small internal inset (must not touch the left edge).
- Avoid a visible outer page frame at the browser edge.
- Left alignment rule: the left edge of the header icon, the status-strip text, and the left edge of lower widget content must be vertically aligned to the same inset line.

## 4. Top Header (Current React Variant)

Canonical header behavior is specified in:

- `docs/ui_header_spec.md`

- Left area:
  - App icon.
  - White, bold "camflow" label.
  - Version directly after camflow in the same line (compact header).
  - Version styling:
    - Primary version token (e.g. v0.1.0): brighter/highlighted.
    - Remaining version text: darker/subdued.
- Right area:
  - Only Editor/Viewer mode switch.
- Explicitly removed from top header:
  - Runtime status text.
  - `reload graph` / `reload parameters` button.
  - `stop pipeline` / `start pipeline` button.

## 5. Runtime Window Model

Canonical editor panel behavior is specified in:

- `docs/ui_editor_panel_spec.md`

- Runtime is represented as an independent floating window in the editor canvas.
- Runtime windows are draggable.
- Runtime windows are resizable.
- Runtime windows must not overlap.
- Runtime canvas supports context menu over the full runtime area.

## 6. Runtime Header Layout

- Header left side:
  - SVG runtime icon (instead of text label "runtime").
  - Immediately to the right: runtime label (IP or host name).
  - Runtime status badge appears directly next to runtime label.
  - Status badge shape: oval.
  - Status colors:
    - Running: green
    - Stopped: blue
    - Down: red
- Header right side:
  - Start/Stop button.

## 7. Runtime Button Semantics

- Button label must be:
  - Start when runtime is not running.
  - Stop when runtime is running.
- Clicking Start/Stop must immediately update the local runtime UI state after API response.
- Start/Stop click must not be swallowed by header drag interactions.

## 8. Runtime Address Editing

- Runtime address/host in header supports inline editing.
- Enter commits the change.
- Escape cancels the edit.
- Blur also commits when value is valid.

## 9. Runtime Defaults

- New runtime defaults to localhost or machine host name.
- Runtime creation dialog includes editable IP/host field.

## 10. Node Interaction Rules

- Nodes are draggable inside their runtime window.
- Nodes must not overlap each other.
- Node position remains stable after runtime updates.
- Node and runtime click interactions must remain functional while drag is enabled.

## 11. Drag/Resize Stability

- Drag and resize must stop reliably when releasing pointer.
- Losing pointer focus (blur, mouse leave) must cancel active drag state.
- Interaction handlers must not create sticky-drag behavior.
- Interactive controls inside draggable headers (buttons/inputs) must not accidentally start drag.

## 12. Streaming/Runtime Stop Behavior

- Stopping runtime should stop ongoing image transfer updates in the viewer path.
- Frame request logic should respect runtime stopped/down state.

## 13. Auto-ID Visibility

- Internal auto-generated marker ID (__auto__) is implementation detail.
- Such markers must never appear as visible runtime windows or user-facing labels.

## 14. Runtime Status Semantics

- Local runtime status uses runtime API state.
- Non-local runtime windows may use a separate style/state representation (e.g. remote).

## 15. Ongoing Documentation Rule

- Every future UI design clarification from user discussions must be appended here.
- Keep this file as a chronological and implementation-oriented design reference for React UI.
- Do this automatically during implementation work; no explicit reminder from user is required.

## 16. Screenshot Parity: Legacy Header and ImageViewer

Canonical viewer panel behavior is specified in:

- `docs/ui_viewer_panel_spec.md`

This section captures the explicit visual reference from attached legacy UI screenshots and must be treated as high-priority for React parity.

### 16.1 Header (Legacy Parity, historical reference)

- Header layout:
  - Left cluster: app icon, white bold "camflow", runtime state badge, version string.
  - Right cluster: legacy variant included action buttons, but this is superseded by section 4 for current implementation.
- Version styling:
  - `v0.1.0` highlighted brighter.
  - Remaining part (`| opencv ...`) subdued/muted.
- Runtime state badge in header:
  - Oval pill.
  - Running = green style.
  - Stopped = blue/neutral style.
  - Down = red style.
- Header and status-strip separators remain full-width (edge-to-edge), while content keeps a consistent inset line.

### 16.2 Status Strip (Legacy Parity)

- Single-line status message below header.
- Left-aligned to same inset as header icon and lower content.
- Top and bottom separator lines clearly visible across full viewport width.

### 16.3 ImageViewer Controls (Legacy Parity)

- Controls are overlaid inside the image frame, top-right.
- Three circular controls in one row:
  - `MSB` chip.
  - Debayer toggle (icon-only RGB dots).
  - Reset/fit icon button.
- Bitshift slider panel:
  - Appears as a floating pill panel near the overlay controls.
  - Contains `bitshift` label and slider.
  - Opens on hover of the control cluster and closes on mouse leave.
- Metadata overlay in image frame (top-left):
  - `seq`, `ts`, format/dimensions.
  - capture/render FPS line.

## 17. Viewer Mode Pane Arrangement (Current Requirement)

- Viewer mode must place panes as follows:
  - Top-left: FrameContext/ImageViewer panel.
  - Top-right: Parameter panel.
  - Bottom-left: Editor panel.
- Right-bottom area is not used by a main panel in this arrangement.

## 18. Image Aspect Ratio Rule (Current Requirement)

- The image display area must always remain rectangular.
- Viewer frame box aspect ratio must follow the received image dimensions (`width/height`).
- Default fallback ratio before first frame is `4:3`.

## 19. Pixel Match Policy (Latest)

- The header **content model** follows section 4 (new definition by user).
- Header **visual style** may follow legacy spacing/color language, but do not reintroduce removed header content.
- All non-header areas (viewer frame, overlays, parameter panel arrangement, controls) should be matched as closely as possible to legacy screenshot proportions and spacing.

## 20. Content Consistency Across Modes

- Editor, FrameContext (viewer), and Parameter panels contain the same information across modes.
- Modes only change panel arrangement, not panel content.
- In viewer mode, the parameter header still shows the selected node name and the same related controls/content as in editor mode.

## 21. Parameter Tooltip-Only Metadata

The detailed and current parameter panel specification was moved to:

- `docs/ui_parameter_panel_spec.md`

This includes the latest runtime update semantics:

- selected node persistence with fallback behavior
- full-reload triggers only on selection, reload button, or `hasSideEffects`
- no runtime update for unchanged values
- integer slider throttling and integer input commit behavior
