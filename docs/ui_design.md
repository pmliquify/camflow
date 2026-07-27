# camflow Web UI Design

This document is the detailed visual and interaction specification for the built-in camflow web UI.

## 1. Design direction

The web UI uses a technical dark theme with cyan/blue accents.

- Goal: fast diagnostics for live image pipelines with minimal visual noise.
- Tone: compact, precise, instrument-like UI.
- Typography: one consistent font family across the whole UI (`Rajdhani`).

## 2. Color and typography system

### 2.1 Core colors

- App background gradient:
	- `#080f1b` -> `#111b2e`
- Panel border:
	- `#223a5f`
- Main text:
	- `#e8f1ff`
- Muted text:
	- `#95a9c8`
- Accent:
	- `#02d7ff`

### 2.2 Status colors

- Runtime running badge:
	- background `#133a24`
	- border `#2d9a58`
	- text `#d6ffe5`
- Runtime down badge:
	- background `#3f1523`
	- border `#7c2236`
	- text `#ffd0d8`
- Offline status-strip:
	- background `rgba(140, 25, 40, 0.35)`
	- border `#7c2236`

### 2.3 Font sizes

- App name (`camflow`): 26px
- Normal controls and labels: 11px to 13px
- Graph text: 12px
- Status strip: 12px

## 3. Global layout

Desktop layout is two-column:

- Left (`~1.6fr`): ImageView + GraphView
- Right (`min 360px`): ParameterView

Mobile (`max-width: 860px`) switches to one column.

All panels use:

- rounded corners (`14px`)
- dark layered fill
- soft depth shadow

## 4. Top row (AppHeader)

Detailed header behavior is maintained in:

- `docs/ui_header_spec.md`

## 5. Status strip

A thin strip below AppHeader communicates transport/runtime state.

Examples:

- `waiting for websocket data`
- `frame websocket connected`
- `service offline - waiting for reconnect`

## 6. ImageView

Detailed viewer panel behavior is maintained in:

- `docs/ui_viewer_panel_spec.md`

### 6.1 Base frame area

- 4:3 viewport with rounded border
- canvas rendering for converted pixel output
- waiting dummy placeholder until first frame

### 6.2 Metadata overlay (top-left)

Always shown over the image frame:

- line 1: `seq: #<seq> | ts: <ts> ms`
- line 2: `capture fps: <fps> | render fps: <fps>`

### 6.3 ImageViewControll (top-right)

This is the compact control area over ImageView.

Always visible:

- `reset` button (resets zoom + pan)

Context dependent (by pixel format):

- Shift chip:
	- `MSB` when slider value = 8
	- `shift <x> LSB` when slider value < 8
- RGB debayer toggle button:
	- icon-only: three colored dots (R/G/B)
	- active state uses highlighted border/inset
- bitshift slider panel (small, fine-grained style)

Behavior:

- slider panel opens on hover over control cluster
- slider panel closes on mouse leave

## 7. Zoom and pan behavior

- Mouse wheel zoom range: `1.0x` to `24.0x`
- Right mouse button drag pans the zoomed image
- Context menu is disabled over ImageView (right-drag reserved for panning)
- `reset` button resets transform to fit view

Important stability rule:

- incoming live frames must not force-fit reset while format stays unchanged
- zoom is preserved during live streaming

## 8. Rendering and conversion pipeline

Image transport and conversion strategy:

- frame data fetched via `/ws/frame` as binary packets
- conversion done client-side for supported formats
- no server-side JPEG dependency in the web UI path

Conversion cache rule:

- if no new frame content is present (same sequence/format/bitshift/debayer state), reuse already converted RGBA buffer
- avoids unnecessary re-conversion work and keeps interaction smooth while zooming

## 9. ParameterView (right panel)

The detailed parameter panel specification was moved to:

- `docs/ui_parameter_panel_spec.md`

## 10. GraphView (below ImageView)

Detailed editor panel behavior is maintained in:

- `docs/ui_editor_panel_spec.md`

## 11. Runtime state synchronization

Polled state endpoint:

- `/api/runtime`

Rules:

- `running`: green badge style
- `stopped`: neutral badge style
- `down`: red badge style + offline strip

## 12. Accessibility and interaction notes

- Debayer button is an actual toggle button with `aria-pressed`
- reset and control buttons are keyboard-focusable standard buttons
- overlays are designed for high contrast on live imagery
