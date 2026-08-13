# camflow UI Specification: Parameter Panel

This document is the canonical specification for the Parameter Panel used in the React web UI.

## 1. Scope and ownership

- Component: `ParameterPanel` and `ParameterRow`.
- Runtime integration owner: `App` state and runtime API functions.
- This document supersedes parameter panel descriptions that were previously spread across:
  - `docs/web_ui.md`,
  - `docs/ui_design.md`,
  - `docs/ui_react_design.md`.

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

### 4.4 Runtime editability

- While the runtime is running, controls with `runtimeWritable=false` are disabled.
- Controls with `runtimeWritable=true` remain editable while the runtime is running.
- When the runtime is stopped, all parameter controls are enabled.
- Local draft parameters without runtime schema metadata remain editable.

## 5. Side effect schema semantics

### 5.1 Schema field

- Field name: `hasSideEffects`.
- Source: runtime parameter schema (`ParameterInfo`) and REST serialization.
- Meaning: changing this parameter can invalidate other parameter entries, requiring a full refresh.

### 5.2 Current known side-effecting parameters

- For `v4l2src`: `device`, `subdevices` are marked with `hasSideEffects=true`.
- All other parameters default to `false` unless explicitly declared.

## 6. UI structure

### 6.1 Header row

- Left: selected node display name (`name` or `id`).
- Fallback when no selected node metadata: selected runtime label.
- Right: `filter` and `reload` buttons.
- When a media entity, pad or link is selected, the left side shows its
  inspection title and the runtime-parameter reload action is hidden.

### 6.2 Filter controls

- The `filter` button toggles parameter-filter edit mode.
- The `reload` button keeps its existing behavior and remains independent from filter state.
- The `filter` button shows an active highlight whenever one or more parameters are currently hidden by checkbox state.

### 6.3 Scroll area

- Vertical scroll container with transient visual scrollbar behavior.
- Wheel events are captured to avoid unintended parent-pan behavior.

### 6.4 Error status

- Parameter update failures are shown in the header status line.
- No separate error line is rendered below the parameter list.

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
- Parameters with a group are preceded by a group separator using the group
  description when available.

### 7.2 Type to control mapping

- `bool` -> checkbox
- `option` -> select
- `button` -> trigger button
- `int` -> slider + numeric input
- `double` -> slider + numeric input
- `string` -> text input
- `media-flags` -> read-only hexadecimal value and named flag states

## 8. Edit and commit behavior

### 8.1 Immediate commit types

- `bool`, `option`, `button`, and `double` commit directly.
- String/default text input commits on `Enter` or blur.

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

## 10. Runtime and media actions

- When no node is selected and the selected runtime is non-local, the panel
  offers the runtime delete action.
- Selecting a media graph entity, pad or link replaces node parameters with a
  read-only detail list.
- Entity details include function, pad count and entity flags.
- Pad details include kernel id and pad flags.
- Link details include kernel id and link flags; the title identifies source
  and sink entities.
- Media flag rows show the hexadecimal value and individual set/unset flag
  states. They never invoke `updateParameter`.

## 11. Parameter filter behavior (UI-only)

### 11.1 Scope

- The filter is strictly a UI presentation feature.
- It must not change runtime values, runtime schema, or runtime API behavior.

### 11.2 Search field semantics

- Label/placeholder text: `search by parameter name`.
- Search is a helper for finding rows while filter edit mode is open.
- Search is not the persistent visibility filter itself.
- Matching covers internal name, display name, group name and group
  description, case-insensitively.

### 11.3 Edit mode list behavior

- While filter edit mode is open:
  - all parameters are shown by default,
  - except when search text is entered, in which case only name matches are shown.
- Unchecking a row's visibility checkbox in edit mode does not immediately remove that row from the current edit-mode list.

### 11.4 Applied visibility behavior

- After leaving filter edit mode, the stored checkbox state is applied as the actual visibility filter.
- Rows with unchecked visibility are hidden from the normal parameter list.

### 11.5 Actions in filter panel

- `clear filter`:
  - clears search text,
  - resets all parameter visibility checkboxes to checked,
  - resulting in all parameters visible.
- `deselect all`:
  - unchecks all parameter visibility checkboxes,
  - resulting in no parameters visible outside edit mode.

### 11.6 Persistence

- Filter state is persisted in browser local storage.
- Storage key: `camflow:param-filter:v1`.
- Persisted fields include:
  - search text,
  - per-parameter visibility map.
- The persisted state is restored on subsequent UI loads in the same browser profile.

## 12. API contract summary

- Read: `GET /api/nodes/{nodeId}/parameters`
- Write one value: `PUT /api/nodes/{nodeId}/parameters/{name}`

Expected parameter payload fields consumed by UI:

- `name`, `type`, `description`, `value`, `default`, `min`, `max`
- `runtimeWritable`, `configured`
- `options`, `optionLabels`
- `origin`, `source`
- `hasSideEffects`

## 13. Non-goals

- Automatic periodic full reload is not part of this panel.
- Global runtime status polling is not a panel concern.
- Pipeline graph persistence is outside panel responsibility.