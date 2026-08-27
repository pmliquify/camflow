# CamFlow Version History

## v0.2.0 (Runtime Diagnostics and Viewer Improvements)
* New Features
  * Added a device tree inspector to the runtime web UI.
  * Added a kernel module debug-level viewer to the runtime web UI.
* Improvements
  * Preserved the last captured frame when the runtime is stopped.
  * Improved display and handling of mixed pixel formats from `filesrc` directories.
  * Standardized pixel format names to fourcc values in the UI and CLI.
  * Improved format error reporting in the viewer.
  * Improved V4L2 format initialization so requested formats are applied reliably after boot.
* Bugfixes
  * Fixed keyboard shortcuts being triggered while editing parameter fields.
  * Fixed frame context subscriptions remaining focused on a previously selected node or image.
  * Fixed V4L2 pixel formats not being applied when the driver required repeated format negotiation.

## v0.1.0 (Initial Runtime Release)
* New Features
  * Introduced the Linux `camflow` runtime for camera pipelines on x86_64 and ARM64.
  * Added pipeline execution with sources, processors and sinks.
  * Added pipeline expressions and JSON graph files with graph edges and input bindings.
  * Added V4L2 camera input with device, subdevice, format and control parameters.
  * Added file input and output, TCP streaming, and runtime logging nodes.
  * Added image conversion support based on OpenCV 4.12 with statically linked OpenCV modules.
  * Added CLI controls for frame limits, profiling, simple-pipeline execution and log sources.
  * Added an integrated web UI with live image viewing and runtime parameter editing.
  * Added REST and WebSocket interfaces for runtime control, status, graph inspection and frame context.
  * Added local development, ARM64 cross-build, deployment and UI/runtime task workflows.
