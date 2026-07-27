# camflow UI Specification: Header

This document specifies the top header (`GlobalHeader`) behavior and content.

## 1. Scope

- Component: `GlobalHeader`.
- Integrates runtime status display, start/stop action, and view-mode switch.

## 2. Layout

- Left cluster:
  - Logo mark (SVG).
  - Product label `camflow`.
  - Runtime status pill.
  - Runtime version text split into primary and secondary segment.
- Right cluster:
  - Runtime toggle button (`Start`/`Stop`).
  - Mode switch (`viewer`, `editor`).

## 3. Runtime status display

- Status text values:
  - `running`
  - `stopped`
  - `down`
- Badge style follows semantic status classes.

## 4. Runtime toggle behavior

- Button label is derived from current running state:
  - Running -> show `Stop`
  - Not running -> show `Start`
- Click invokes the runtime toggle callback.
- Header interaction must not bubble and close editor context menus accidentally.

## 5. Mode switch behavior

- Two explicit mode buttons:
  - `viewer`
  - `editor`
- Active mode button has active visual state.
- Clicking a mode button updates persisted view mode in app state.

## 6. Content constraints

- No `start all` or `stop all` actions in the global header.
- No aggregated runtime counters in the global header.
- Node-level or graph-level editing actions do not belong in this component.

## 7. Data inputs

`GlobalHeader` consumes:

- `runtimeStatusText`
- `versionParts` (`primary`, `secondary`)
- `runtimeRunning`
- `onToggleRuntime`
- `viewMode`
- `onSetViewMode`

## 8. Accessibility

- Interactive elements use semantic `button` controls.
- Mode switch remains keyboard-operable.