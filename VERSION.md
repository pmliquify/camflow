# CamFlow Version History

## v0.5.2

### Added

- Built-in web UI mode with browser preview and runtime `v4l2src` parameter updates.
- Runtime-only repository layout.

### Changed

- OpenCV 4.12 is required and linked statically.
- CI and release builds target Linux x86_64 and Linux arm64 runtime artifacts.
- REST API and web UI documentation aligned with implementation.

## v0.5.1

### Changed

- V4L2 handling refactored into dedicated `V4L2Device`/control helpers.
- Node runtime parameter access unified through `Node::parameter*` helpers.

## v0.5.0

### Added

- Runtime graph execution model with sources, processors, probes and sinks.
- REST runtime control endpoint and JSON/CLI graph parsing.
