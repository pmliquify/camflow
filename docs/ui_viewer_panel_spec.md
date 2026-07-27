# camflow UI Specification: Viewer Panel

This document specifies the frame/context viewer panel (`FrameViewerPanel`).

## 1. Scope

- Component: `FrameViewerPanel`, including frame viewport and frame-context value area.
- Integrates image rendering, overlay controls, and metadata display.

## 2. Data sources

- Frame context and frame binary websocket: `/ws/frame`

Frame rendering is client-side from raw packets. Context metadata and matching image packets are emitted in order on the same stream.

## 3. Frame viewport behavior

- Aspect ratio follows current frame dimensions (`width/height`).
- Fallback ratio before first frame is `4:3`.
- Canvas is transformed for zoom and pan.

## 4. Overlays and metadata

- Sequence and timestamp display in overlay.
- Capture FPS and render FPS display in overlay.
- Compact metadata grid shown when frame metadata is available.

## 5. Controls

- Reset view control restores default viewer transform.
- Bitshift control for formats requiring bit alignment.
- Debayer toggle for Bayer formats.

Control visibility and behavior are format-aware.

## 6. Interaction

- Mouse wheel: zoom.
- Right or middle mouse drag: pan.
- Pan gesture uses thresholding to avoid accidental movement.

## 7. Runtime state coupling

- Viewer stream is active only while runtime status is `running`.
- On stopped/down transitions, websocket connections are closed.

## 8. Rendering performance rules

- Last frame packet processing is serialized to avoid concurrent conversion.
- Converted RGBA cache may be reused for identical render conditions.
- Frame request flow uses explicit `next` requests to avoid uncontrolled stream burst.

## 9. Error handling

- Invalid websocket payloads surface as editor/UI error state.
- Unsupported conversion paths do not crash panel; they report a conversion error.