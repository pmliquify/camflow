# camflow UI Specification: Header

This document is the canonical specification for the top header (`GlobalHeader`).

## 1. Scope

- Component: `GlobalHeader`.
- Integrates runtime status display, start/stop action, and view-mode switch.

## 2. Layout

- Left cluster:
  - Logo mark (SVG).
  - Product label `camflow`.
  - Compact primary runtime version directly after the product label.
  - Runtime status pill.
  - Graph/runtime status text.
- Right cluster:
  - Runtime toggle button (`Start`/`Stop`).
  - Mode switch (`viewer`, `editor`).

The complete secondary version string is shown in a tooltip when the compact
version receives hover or keyboard focus.

## 3. Runtime status display

- Status text values:
  - `running`
  - `stopped`
  - `down`
- Badge style follows semantic status classes.
- `running` uses the success tone, `stopped` the information tone, and `down`
  the danger tone.

## 4. Graph and error status

- The text after the runtime status pill reports graph discovery or the latest
  editor/runtime operation status.
- The full text is available through its tooltip when it is visually clipped.
- An active editor error replaces the normal discovery text and applies the
  error visual state.

## 5. Runtime toggle behavior

- Button label is derived from current running state:
  - Running -> show `Stop`
  - Not running -> show `Start`
- Click invokes the runtime toggle callback.
- Header clicks do not bubble into the application-level context-menu closer.

## 6. Mode switch behavior

- Two explicit mode buttons:
  - `viewer`
  - `editor`
- Active mode button has active visual state.
- Clicking a mode button updates persisted view mode in app state.
- Browser storage key: `camflow:view-mode`.

## 7. Content constraints

- No `start all` or `stop all` actions in the global header.
- No aggregated runtime counters in the global header.
- Node-level or graph-level editing actions do not belong in this component.

## 8. Data inputs

`GlobalHeader` consumes:

- `runtimeStatusText`
- `versionParts` (`primary`, `secondary`)
- `graphStatusText`
- `statusError`
- `runtimeRunning`
- `onToggleRuntime`
- `viewMode`
- `onSetViewMode`

## 9. Accessibility

- Interactive elements use semantic `button` controls.
- Mode switch remains keyboard-operable.
- The compact version is focusable and exposes the full version through a
  tooltip with an accessible label.