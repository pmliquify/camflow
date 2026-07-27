# camflow UI Specification: Parameter Panel

This document is the canonical specification for the Parameter Panel used in the React web UI.

## 1. Scope and ownership

- Component: `ParameterPanel` and `ParameterRow`.
- Runtime integration owner: `App` state and runtime API functions.
- This document supersedes parameter panel descriptions that were previously spread across:
  - `docs/web_ui.md` (`Parameter model` section)
  - `docs/ui_design.md` (`9. ParameterView` section)
  - `docs/ui_react_design.md` (`21. Parameter Tooltip-Only Metadata` and related behavior notes)

## 2. Purpose

The Parameter Panel provides a compact editor for the currently selected node's runtime parameters.

- Show parameter schema and current value.
- Allow editing by type-specific controls.
- Minimize runtime traffic by only sending real value changes.
- Trigger full parameter reload only in explicitly defined cases.

## 3. Selection and load lifecycle

### 3.1 Selected node persistence

- Browser storage key: `camflow:selected-node-id`.
- On UI load:
  - If saved node exists in the current pipeline, select it.
  - Else select the first pipeline node (if available).
  - Else no node is selected.
- On each node selection change, persist the selected node id.

### 3.2 Full parameter reload triggers

A complete parameter schema/value reload for one node is allowed only in the following cases:

- A node is selected (or selection changes).
- The user clicks the `reload` button in the Parameter Panel.
- A parameter update has `hasSideEffects=true` in schema metadata.

Outside these three cases, no full parameter reload must happen.

## 4. Runtime update policy

### 4.1 General rule

- Only send parameter updates to runtime when the value effectively changed.
- Never send updates for no-op edits.

### 4.2 Equality check semantics

Comparison is type-aware before sending:

- `bool`: normalize values to boolean (`true/false`, including textual forms like `1/yes/on`).
- `int`: compare normalized integer values.
- `double`: compare normalized floating-point values.
- `string` and `option`: compare normalized string values.

No runtime request is sent if normalized next value equals normalized current committed value.

### 4.3 Committed value source

- The UI maintains a committed-value cache from successful runtime reads/writes.
- No-op suppression compares against the committed value, not only against temporary input state.
- Pending delayed updates (for slider interactions) are cancelled if value returns to committed value.

## 5. Side effect schema semantics

### 5.1 Schema field

- Field name: `hasSideEffects`.
- Source: runtime parameter schema (`ParameterInfo`) and REST serialization.
- Meaning: changing this parameter can invalidate other parameter entries, requiring a full refresh.

### 5.2 Current known side-effecting parameters

- For `v4l2src`: `device`, `subdevice` are marked with `hasSideEffects=true`.
- All other parameters default to `false` unless explicitly declared.

## 6. UI structure

### 6.1 Header row

- Left: selected node display name (`name` or `id`).
- Fallback when no selected node metadata: selected runtime label.
- Right: `filter` and `reload` buttons.

### 6.4 Filter controls

- The `filter` button toggles parameter-filter edit mode.
- The `reload` button keeps its existing behavior and remains independent from filter state.
- The `filter` button shows an active highlight whenever one or more parameters are currently hidden by checkbox state.

### 6.2 Scroll area

- Vertical scroll container with transient visual scrollbar behavior.
- Wheel events are captured to avoid unintended parent-pan behavior.

### 6.3 Error line

- When update/read fails, an error message is shown below the list.

## 7. Parameter row rendering

### 7.0 Visibility checkbox mode

- In filter edit mode, each parameter row shows a visibility checkbox in front of the parameter name.
- Outside filter edit mode, these checkboxes are hidden.
- Checkbox state determines which parameters are visible outside edit mode.

### 7.1 Tooltip and metadata

- Tooltip format:
  - `<description> (<type>)`
  - If missing description: `no description (<type>)`
  - Append optional metadata parts: `source: ...`, `origin: ...`
- Dynamic-origin parameters may show a compact origin badge (for example `v4l2`).

### 7.2 Type to control mapping

- `bool` -> checkbox
- `option` -> select
- `button` -> trigger button
- `int` -> slider + numeric input
- `double` -> slider + numeric input
- `string` -> text input

## 8. Edit and commit behavior

### 8.1 Immediate commit types

- `bool`, `option`, `button`, `double`, and default text field behavior use direct commit events.

### 8.2 Integer behavior (strict)

- Slider interaction:
  - UI value updates immediately for responsiveness.
  - Runtime updates are throttled to one send per 200 ms window.
- Numeric input interaction:
  - Keystrokes update local draft only.
  - Runtime update occurs only on `Enter` or `blur`.

## 9. Local-only nodes

If selected node is not a live runtime node:

- Panel shows local/draft parameter representation.
- No runtime parameter write is attempted.

## 12. Parameter filter behavior (UI-only)

### 12.1 Scope

- The filter is strictly a UI presentation feature.
- It must not change runtime values, runtime schema, or runtime API behavior.

### 12.2 Search field semantics

- Label/placeholder text: `search by parameter name`.
- Search is a helper for finding rows while filter edit mode is open.
- Search is not the persistent visibility filter itself.

### 12.3 Edit mode list behavior

- While filter edit mode is open:
  - all parameters are shown by default,
  - except when search text is entered, in which case only name matches are shown.
- Unchecking a row's visibility checkbox in edit mode does not immediately remove that row from the current edit-mode list.

### 12.4 Applied visibility behavior

- After leaving filter edit mode, the stored checkbox state is applied as the actual visibility filter.
- Rows with unchecked visibility are hidden from the normal parameter list.

### 12.5 Actions in filter panel

- `clear filter`:
  - clears search text,
  - resets all parameter visibility checkboxes to checked,
  - resulting in all parameters visible.
- `deselect all`:
  - unchecks all parameter visibility checkboxes,
  - resulting in no parameters visible outside edit mode.

### 12.6 Persistence

- Filter state is persisted in browser local storage.
- Storage key: `camflow:param-filter:v1`.
- Persisted fields include:
  - search text,
  - per-parameter visibility map.
- The persisted state is restored on subsequent UI loads in the same browser profile.

## 10. API contract summary

- Read: `GET /api/nodes/{nodeId}/parameters`
- Write one value: `PUT /api/nodes/{nodeId}/parameters/{name}`

Expected parameter payload fields consumed by UI:

- `name`, `type`, `description`, `value`, `default`, `min`, `max`
- `runtimeWritable`, `configured`
- `options`, `optionLabels`
- `origin`, `source`
- `hasSideEffects`

## 11. Non-goals

- Automatic periodic full reload is not part of this panel.
- Global runtime status polling is not a panel concern.
- Pipeline graph persistence is outside panel responsibility.