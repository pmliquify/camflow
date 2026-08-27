# CamFlow Specification v0.5.0

This document is the normative entry point for CamFlow.

## 1. Scope

CamFlow is a runtime-only Linux project.

The runtime executable is `camflow` and supports:

- Linux x86_64
- Linux arm64

## 2. Runtime responsibilities

The runtime provides:

- graph-based image processing pipeline execution
- V4L2 camera capture
- REST API for runtime introspection and control
- dedicated web UI mode for live preview and `v4l2src` parameter control

## 3. Dependency policy

OpenCV 4.12 is required and linked statically into the runtime binary.

Only required OpenCV modules are built:

- `core`
- `imgproc`
- `imgcodecs`

GStreamer is optional and only used for relevant source nodes.

## 4. Graph model

A pipeline consists of source, processor and sink nodes.

- Graph input: CLI expression or JSON graph
- Runtime value exchange: `FrameContext`
- Node-owned runtime settings: `Node` parameter storage

## 5. FrameContext policy

`FrameContext` carries inter-node payloads and scoped metadata.

Node-owned parameters are not published into `FrameContext` by the scheduler.
Nodes read their own parameters directly through `Node::parameter*` helpers.

## 6. Interfaces

- REST API: [rest_api.md](rest_api.md)
- Web UI service: [web_ui.md](web_ui.md)

## 7. Related documents

- [architecture.md](architecture.md)
- [nodes.md](nodes.md)
- [frame_context.md](frame_context.md)
- [scheduler.md](scheduler.md)
- [image_conversion.md](image_conversion.md)
- [graph_json.md](graph_json.md)
- [debugging.md](debugging.md)
- [development.md](development.md)
- [driver_release_test_automation.md](driver_release_test_automation.md)
