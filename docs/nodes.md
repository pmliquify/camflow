# Nodes

Nodes are the executable units of a CamFlow graph. They are divided into Sources, Processors, Probes and Sinks.

## Runtime Parameter Model

- `NodeSchema` is the node contract and is split into `parameters`, `inputs` and `outputs`.
- Parameters are node-owned settings stored in `Node` and read via `parameter*` helper methods.
- Inputs are explicit binding targets and describe expected data types.
- Outputs describe values a node can publish (for example `image`).
- Nodes read node-owned runtime values directly from `Node` during processing.
- Node-owned configured values are read through `Node::parameter(...)` and typed
	accessors (`parameterBool`, `parameterInt`, `parameterDouble`, `parameterString`).
- Input bindings are configured in the pipeline expression as `<input>=<nodeId>.<output>`.
- Inputs may allow multiple bindings (for example compositor image input).

## Naming

Factory names are lowercase. User-facing source names use `src` instead of `source`.

Examples:

- `v4l2src`
- `nvargussrc`
- `filesrc`
- `compositor`
- `probe`
- `filesink`
- `tcpsink`

## Runtime nodes

### V4L2Source

Factory name: `v4l2src`

Linux-only V4L2 image source. It uses the adapted implementation from the earlier `v4l2-test` project.

`V4L2Source` is intentionally a thin node implementation. V4L2 API operations,
streaming state, buffer queueing and frame wait/dequeue logic are encapsulated in
`V4L2Device`; control discovery and read/write are handled by `V4L2ControlAccess`.

Always available parameters:

- `device`
- `subdevices`
- `pixelformat`
- `width`
- `height`

V4L2 controls from both device and all selected subdevices are enumerated dynamically and exposed as runtime parameters when the device is available.

Subdevice controls are exposed only for selected `subdevices` and are grouped and prefixed by subdevice index:

- group name: `subdevN` (example: `subdev3` for `/dev/v4l-subdev3`)
- parameter name: `subdevN.<control>` (example: `subdev3.exposure`)

If no subdevices are selected, no dynamic subdevice controls are exposed.

Options for devices and subdevices are shown as `/dev/... (<name>, <version>)` and `pixelformat` is populated from the device-supported format list.

### BitShiftProcessor

Factory name: `bitshift`

Applies a configurable right shift (`shift` from 0 to 8) to raw Bayer input and converts the result to `BGR888`.

This node is intended for raw camera graphs that need explicit bit-depth normalization before color conversion.

### TCPSink

Factory name: `tcpsink`

Sends the current image frame to a TCP image receiver using the legacy image protocol.

Parameters:

- `ip`
- `port`
- `reconnect`
- `transport` (`image` or `framecontext`)
- `contextKeys` (comma-separated keys used in `framecontext` mode)

`tcpsink` remains available as an explicit node for direct socket streaming workflows.

## Transparent Cross-Runtime Edges

For supergraph usage, runtime-to-runtime transport is integrated into the runtime scheduler.

- Users create normal node-to-node edges across runtimes.
- The runtime keeps these edges in the graph model unchanged.
- Data transfer between hosts is handled transparently by the runtime and does not require explicit `tcpsink`/`tcpsrc` nodes.
- Multiple concurrent cross-runtime edges are supported, including multiple edges between the same runtime pair.

This keeps graph authoring user-focused while preserving explicit node-level connectivity.

### Probe

Factory name: `probe`

The CLI pipeline syntax supports probes through arrow operators like `-captureA>` and materializes them as regular `probe` nodes in the runtime graph.

Example:

```text
v4l2src -captureA> tcpsink
```

This inserts a probe node that prints one line per processed image frame, including sequence, timestamp, delta time, dimensions, stride and pixel format.
The probe id is an arbitrary non-empty string and becomes the node id of the inserted probe.

## Image processing nodes

### FileSource

Factory name: `filesrc`

Loads RAW, PNG and JPG files. For RAW files no pixel conversion is performed in the source node; bytes are forwarded as-is in an `ImageBuffer`.

Supported RAW formats:

- `GREY`
- `RGGB`, `GBRG`, `GRBG`, `BGGR`
- `Y10`, `Y10P`
- `RG10`, `GB10`, `GR10`, `BG10`
- `RG10P`, `GB10P`, `GR10P`, `BG10P`
- `Y12`, `Y12P`
- `RG12`, `GB12`, `GR12`, `BG12`
- `RG12P`, `GB12P`, `GR12P`, `BG12P`
- `Y14` (mono 14-bit)
- `RG14`, `GB14`, `GR14`, `BG14`
- `RG14P`, `GB14P`, `GR14P`, `BG14P`
- `YUYV`, `NV12`

Behavior details:

- RAW requires width and height unless filename contains a token like `...1920x1080...`.
- Explicit `width`/`height` parameters override filename dimensions.
- Stride is auto-derived from format unless `stride` is explicitly set.
- `bitShift` stores right-shift metadata in `ImageBuffer` and is applied in RAW conversion paths before automatic debayering.
- For `YUYV` the default stride is `width * 2`.
- For `NV12` the default stride is `width`; file size is interpreted as Y plane plus interleaved UV plane.
- In directory mode, `wildcard` filters file names (for example `frame_*_left*.raw`).
- With `repeat=true`, source loops and can generate an unbounded stream.
- For PNG/JPG width/height/stride parameters are optional because geometry is read from file metadata.

### FileSink

Factory name: `filesink`

Writes RAW, PNG or JPG. Conversion is deferred until output, so data is converted only when required by the selected output format.

Behavior details:

- RAW output writes bytes directly when input format already matches.
- RAW input to PNG/JPG triggers automatic debayer/conversion using the standard project conversion path.
- Non-RAW input is converted to the target output format when needed.
- Optional `format` parameter defines RAW target format for non-encoded outputs.
- `format=YUYV` and `format=NV12` are supported for raw output files.
- `YUYV` and `NV12` input can be written directly as raw files or converted to PNG/JPG.
- Width and height can be part of the output filename (for example `capture_1920x1080_rg10.raw`) to stay compatible with `filesrc` filename parsing.

### DebayerProcessor

Factory name: `debayer`

Converts Bayer RAW input formats to `BGR888` using OpenCV. The processor rejects non-Bayer input.

### CCMProcessor

Factory name: `ccm`

Applies a 3x3 Color Correction Matrix in BGR space. If input is RAW Bayer, debayering is applied automatically before the matrix operation.

### CompositorProcessor

Factory name: `compositor`

Composites multiple bound images into one image.

Input binding:

- `image=<nodeId>.<output>[,<nodeId>.<output>...]`

Node parameters:

- `xpos` comma-separated x offsets
- `ypos` comma-separated y offsets
- `zorder` comma-separated z values

The input `image` allows multiple bindings. The parameter arrays are mapped by image index.

Defaults:

- `xpos=0`, `ypos=0`, `zorder=0`

## GStreamer-dependent nodes

### NvArgusSource

Factory name: `nvargussrc`

Uses GStreamer to access NVIDIA Argus camera pipelines. It is built only when GStreamer is found.


## v0.5.2 update

`TCPSink` remains the image transport sink for runtime deployments.
