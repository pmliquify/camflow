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
  - Keyboard-shortcut help button and anchored shortcut popover.

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

## 7. Keyboard shortcuts

- `Alt+V` (`Option+V` on macOS) toggles between viewer and editor mode.
- `Alt+R` (`Option+R` on macOS) toggles the local runtime between started and
  stopped.
- `Alt+H` (`Option+H` on macOS) opens the keyboard-shortcut help popover.
- `Alt+F` (`Option+F` on macOS) opens the parameter filter, focuses its search
  field and selects its complete existing value.
- `Alt+K` (`Option+K` on macOS) clears the log entries for the currently
  selected runtime, matching that runtime log window's clear button.
- `Alt+G` (`Option+G` on macOS) opens the selected runtime's log panel and log
  filter, focuses the kernel-regex field and selects its complete existing
  value.
- `Escape` closes the parameter filter and all runtime-log filter modes without
  clearing their configured filter values.
- `Tab` remains reserved for normal focus navigation, including parameter
  controls. Only visible, enabled controls within parameter rows participate in
  the `Tab` and `Shift+Tab` sequence. `Space` retains its native behavior in
  editable controls.
- Shortcuts ignore key-repeat events and combinations with additional
  `Ctrl`, `Shift` or `Meta` modifiers.
- Shortcuts are disabled while an application dialog is open.
- The rightmost header button opens a small popover listing both shortcuts.
  It displays the modifier as `⌥` on Apple devices. Clicking outside or
  pressing `Escape` closes it.

## 8. Content constraints

- No `start all` or `stop all` actions in the global header.
- No aggregated runtime counters in the global header.
- Node-level or graph-level editing actions do not belong in this component.

## 9. Data inputs

`GlobalHeader` consumes:

- `runtimeStatusText`
- `versionParts` (`primary`, `secondary`)
- `graphStatusText`
- `statusError`
- `runtimeRunning`
- `onToggleRuntime`
- `viewMode`
- `onSetViewMode`
- `shortcutPanelOpen`
- `onSetShortcutPanelOpen`

## 10. Accessibility

- Interactive elements use semantic `button` controls.
- Mode switch remains keyboard-operable.
- Shortcut help exposes `aria-expanded`, `aria-controls`, a dialog label and
  returns focus to its trigger when closed with `Escape`.
- The compact version is focusable and exposes the full version through a
  tooltip with an accessible label.