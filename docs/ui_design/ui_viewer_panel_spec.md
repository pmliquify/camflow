# camflow UI Specification: Viewer Panel

This document is the canonical specification for `FrameViewerPanel`,
`FrameViewport` and their overlays.

## 1. Scope

- Component: `FrameViewerPanel`, including frame viewport and frame-context value area.
- Integrates image rendering, overlay controls, and metadata display.

## 2. Data sources

- Frame context and frame binary websocket: `/ws/frame`

Frame rendering is client-side from raw packets. Context metadata and matching image packets are emitted in order on the same stream.

- The subscription identifies the selected node and, when present, a nested
	frame-context image key.
- If a context advertises multiple `images`, the UI keeps the requested key
	when available and otherwise selects the first image.
- Context sequence tracking associates the following binary packet with the
	expected metadata and rejects stale/out-of-order work.

## 3. Frame viewport behavior

- Aspect ratio follows current frame dimensions (`width/height`).
- Fallback ratio before first frame is `4:3`.
- The canvas is transformed independently of the fixed frame box.
- When the frame box changes size, including viewer/editor mode switches, the
	canvas transform is reapplied without requiring a new frame. Pan offsets are
	scaled with the fit factor so the same image-space section remains visible at
	the new display scale.
- Before a frame is available, stopped state shows
	`PIPELINE STOPPED - CLICK START PIPELINE`; down/offline state shows
	`SERVICE OFFLINE`.

## 4. Overlays and metadata

- The top-left overlay contains four lines:
	- selected node display name,
	- pixel format label and dimensions,
	- sequence and timestamp in milliseconds,
	- smoothed capture and render FPS.
- Metadata and controls are rendered only after a frame exists.

## 5. Controls

- The top-right reset control restores `1.0x` zoom and zero user pan.
- The debayer icon toggle is shown only for supported Bayer color formats.
- Debayer state persists per selected node in
	`camflow:frame-viewer-settings:v1`.

The bit-shift value is read from websocket frame metadata and applied automatically
before RAW conversion or debayering.

Control visibility and behavior are format-aware.

## 6. Interaction

- Plain wheel zooms from `1.0x` to `24.0x` around the pointer.
- `Shift` + wheel pans one axis and `Alt` + wheel pans the other axis.
- Right or middle drag pans after an 8px threshold.
- Pan continues only while the initiating button remains pressed and stops on
	release or blur; moving after release never changes the transform.
- The native context menu is disabled over the canvas because right drag is
	reserved for navigation.
- Incoming frames preserve zoom and pan while the view remains active.

## 7. Runtime state coupling

- Viewer stream is active only while runtime status is `running`.
- On stopped/down transitions, websocket connections are closed.
- The client sends a `subscribe` command when the socket opens and whenever the
  selected node or image key changes.

## 8. Rendering performance rules

- Frame packet processing is serialized to avoid concurrent conversion.
- Conversion supports Bayer/raw, monochrome, RGB/BGR and YUYV paths used by the
	runtime image-conversion layer.
- `bitShift` metadata is applied before raw normalization or debayering.
- Converted RGBA is reused only when sequence, format, bits-per-pixel, stride,
	bit shift, debayer state and dimensions still match.
- Packets with an unexpected or already-rendered sequence are ignored.

## 9. Error handling

- Invalid websocket payloads surface as editor/UI error state.
- Unsupported conversion paths do not crash panel; they report a conversion error.

## 10. Layout by application mode

- In `viewer` mode the panel occupies the left column beside the Parameter
	Panel and the editor is hidden.
- In `editor` mode the panel occupies the upper-right area above the Parameter
	Panel while the editor spans the left column.
- Panel content and viewer state are the same in both layouts.