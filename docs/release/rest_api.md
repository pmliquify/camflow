# REST API

The REST API is the control interface for runtime automation and tooling.

## Base path

```text
/api
```

For browser-based multi-runtime control, REST responses include permissive CORS headers and support `OPTIONS` preflight.

In UI mode, the root path `/` serves the integrated web UI.

UI mode uses a source-only graph (`v4l2src0`) and captures frame data from runtime FrameContext events.

## Conventions

- All REST payloads are JSON unless stated otherwise.
- `/api/edges` is JSON-only. Plain-text edge payloads (for example `cam0.output -> sink0.input`) are no longer accepted.
- WebSocket payloads use mixed frame types:
  - text frames for JSON metadata
  - binary frames for raw image packets
- Path parameter `{id}` and `{name}` are URL-decoded server-side.

## CORS and transport headers

REST responses include:

- `Access-Control-Allow-Origin: *`
- `Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS`
- `Access-Control-Allow-Headers: Content-Type`
- `Cache-Control: no-store, no-cache, must-revalidate`

`OPTIONS` requests return `204 No Content`.

## API Quick Reference

| Endpoint | Method | Request fields | Response fields |
|---|---|---|---|
| `/api/runtime` | `GET` | none | `state`, `ui` |
| `/api/runtime` | `PUT` | `desiredState` (`running`/`stopped`) | `state`, `ui` |
| `/api/runtime/version` | `GET` | none | `version`, `git`, `build`, `opencv` |
| `/api/media` | `GET` | none | available `devices[]` |
| `/api/media/{device}` | `GET` | path: `{device}` (for example `media0`) | media `entities[]`, pads and `links[]` |
| `/api/devicetree` | `GET` | none | `root`, `node` (recursive), `nodeCount`, `truncated` |
| `/api/moduledebug` | `GET` | none | `modules[]` |
| `/api/moduledebug/{name}` | `PUT` | path: `{name}`; body: new debug level as text | `ok` |
| `/api/pipeline` | `GET` | none | runtime graph JSON (`nodes`, `edges`, ...) |
| `/api/pipeline` | `PUT` / `POST` | graph JSON (`nodes`, `edges`) | `ok` |
| `/api/nodes` | `GET` | none | `sources`, `processors`, `sinks`, `schemas` |
| `/api/nodes` | `POST` / `PUT` | `id`, `type`, optional `runtimeTargetIp` / `runtimeId` / `runtime` | `ok`, `id` |
| `/api/nodes/{id}/id` | `PUT` | path: `{id}`; body: new id as text | `ok`, `id` |
| `/api/nodes/{id}` | `DELETE` | path: `{id}` | `ok` |
| `/api/edges` | `POST` / `PUT` | JSON edge fields | `ok` |
| `/api/edges` | `DELETE` | JSON edge fields | `ok` |
| `/api/nodes/{id}/parameters` | `GET` | path: `{id}` | `parameters[]`, `inputs[]`, `outputs[]` |
| `/api/nodes/{id}/parameters/{name}` | `PUT` / `POST` | path: `{id}`, `{name}`; body: scalar text value | `ok` |
| `/ws/frame` | `GET` (WebSocket upgrade) | subscribe JSON: `cmd`, optional `nodeId`, optional `imageKey` | text JSON: `nodeId`, `images[]`, `values[]`, `keys[]`; binary frame packet |
| `/ws/logs` | `GET` (WebSocket upgrade) | none; connection subscribes to runtime/logger output | text JSON log records (`source`, `type`, `rendered`, `message`, `file`, `line`, `timestampMs`) |

## GET /api/pipeline

Returns the current graph configuration.

Response body is the runtime graph JSON as produced by the runtime controller.

Typical top-level fields:

- `nodes`: array of node objects
- `edges`: array of `fromId.fromPort -> toId.toPort` edge expression strings

See [JSON Graph Format](graph_json.md) for the full document shape.

Status codes:

- `200 OK`

## GET /api/media and /api/media/{device}

`GET /api/media` lists the media-controller devices currently present on the runtime host. `GET /api/media/{device}` reads a fresh kernel topology snapshot for one listed device. The device path is restricted to discovered `/dev/mediaX` entries.

The graph response contains media device metadata, entities with associated `/dev/videoX` or `/dev/v4l-subdevX` nodes, entity pads with active pixel/media-bus format and image size, and pad-to-pad links. Entities, pads, and links expose their kernel `flags` as an unsigned numeric value; derived flag fields are not included.

Status codes:

- `200 OK`: device list or graph snapshot returned
- `404 Not Found`: requested media device is not present
- `500 Internal Server Error`: the media device or topology could not be queried

## GET /api/devicetree

Returns a full snapshot of the flattened device tree exposed by the kernel. The runtime reads `/sys/firmware/devicetree/base`, falling back to `/proc/device-tree`. Symbolic links are skipped so alias shortcuts do not duplicate subtrees.

Response fields:

- `root`: the device tree directory that was read
- `nodeCount`: number of serialized nodes
- `truncated`: `true` when the node limit was reached and the tree is incomplete
- `node`: the root node, recursively containing `name`, `path`, `properties[]` and `children[]`

Each property carries `name`, `length` (bytes read), `truncated` (property longer than 4096 bytes) and a `type` that selects the value field:

- `empty`: zero-length property, no value field
- `string` / `stringList`: `strings[]`
- `cells`: `cells[]` of big-endian unsigned 32-bit values
- `bytes`: `bytes` as a lowercase hex string
- `unreadable`: the property file could not be opened, no value field

Property types are derived heuristically because the flattened device tree does not store them.

Status codes:

- `200 OK`: tree snapshot returned
- `404 Not Found`: the host does not expose a device tree

## GET /api/moduledebug

Lists loaded kernel modules that expose a writable `debug` parameter under `/sys/module/<name>/parameters/debug`.

Response fields:

- `modules`: array of module entries, each with:
  - `name`: `string`
  - `path`: `string` (path to the `debug` parameter file)
  - `value`: `string` (current debug level)
  - `writable`: `boolean`
  - `version`: `string` (module version, empty if not exposed)
  - `refcnt`: `string` (module reference count, empty if not exposed)
  - `initstate`: `string` (module init state, empty if not exposed)
  - `parameters`: `string[]` (other module parameter names)

Status codes:

- `200 OK`

## PUT /api/moduledebug/{name}

Sets a kernel module's debug level.

Request body: the new debug level as plain text (integer, optionally negative).

Response body:

```json
{
  "ok": true
}
```

Status codes:

- `200 OK`: debug level updated
- `400 Bad Request`: invalid module name or non-integer value
- `404 Not Found`: module or debug parameter not found

## GET /api/nodes

Returns registered node type names grouped by source, processor and sink, plus their schemas.

Response fields:

- `sources`: `string[]`
- `processors`: `string[]`
- `sinks`: `string[]`
- `schemas`: object keyed by node type; each value contains `parameters`, `inputs` and `outputs`

Example response:

```json
{
  "sources": ["filesrc", "v4l2src"],
  "processors": ["debayer"],
  "sinks": ["filesink", "logsink", "tcpsink"],
  "schemas": {
    "v4l2src": {
      "parameters": [],
      "inputs": [],
      "outputs": [{"name": "image", "type": "image", "description": "Captured frame"}]
    }
  }
}
```

`sources`, `processors` and `sinks` are returned in alphabetical order and only
list node types actually registered in this build (see [Nodes](nodes.md)).

Status codes:

- `200 OK`

## PUT /api/pipeline

Also accepts `POST /api/pipeline`.

Replaces the full graph configuration.

Request body:

- JSON graph document with top-level `nodes` and `edges`, as described in
  [JSON Graph Format](graph_json.md).

Response body on success:

```json
{
  "ok": true
}
```

Status codes:

- `200 OK`: graph replaced
- `400 Bad Request`: invalid graph JSON
- `500 Internal Server Error`: replace failed

## POST /api/nodes

Adds a single node instance to the existing pipeline graph without replacing the whole graph.

Notes:

- The runtime must be stopped before calling this endpoint.
- Existing node instances remain intact; only the new node is instantiated and appended.

Request body:

```json
{
  "id": "__auto__",
  "type": "debayer"
}
```

Optional runtime-placement metadata may be sent outside `parameters`:

```json
{
  "id": "__auto__",
  "type": "debayer",
  "runtimeTargetIp": "192.168.1.10"
}
```

Implemented request fields:

- `id` (`string`, optional): explicit or auto marker id
- `type` (`string`, required): node type
- `runtimeTargetIp` (`string`, optional)
- `runtimeId` (`string`, optional alias)
- `runtime` (`string`, optional alias)

When `id` is set to `__auto__`, the runtime assigns a unique concrete id for the
new node instance based on the node type and the currently existing graph.

Response body:

```json
{
  "ok": true,
  "id": "debayer0"
}
```

Status codes:

- `200 OK`: node created
- `400 Bad Request`: invalid node payload
- `409 Conflict`: add failed

## POST /api/edges

Adds a single edge to the existing pipeline graph without replacing the whole graph.

Notes:

- The runtime must be stopped before calling this endpoint.
- Existing node instances remain intact; only the new edge is registered.

Request body (JSON object):

```json
{
  "fromNode": "cam0",
  "fromPort": "output",
  "toNode": "debayer0",
  "toPort": "input"
}
```

Alternative JSON request body is also accepted:

```json
{
  "from": "cam0.output",
  "to": "debayer0.input"
}
```

Implemented request fields:

- `fromNode` / `toNode`
- `fromPort` / `toPort`
- alternatively `from` / `to` with `node.port`

Port normalization:

- empty or `output` -> `image` for source side
- empty or `input` -> `image` for target side

Response body:

```json
{
  "ok": true
}
```

Status codes:

- `200 OK`: edge created
- `400 Bad Request`: invalid edge payload
- `409 Conflict`: add failed

Plain-text bodies are rejected with `400 Bad Request`.

## DELETE /api/edges

Removes one edge.

Request body uses the same JSON format as `POST /api/edges`.

Response body:

```json
{
  "ok": true
}
```

Status codes:

- `200 OK`: edge removed
- `400 Bad Request`: invalid edge payload
- `409 Conflict`: remove failed

## DELETE /api/nodes/{id}

Deletes one node by id.

Response body:

```json
{
  "ok": true
}
```

Status codes:

- `200 OK`: node removed
- `409 Conflict`: remove failed

## PUT /api/nodes/{id}/id

Renames one node without replacing or rebuilding the graph. The request body contains only the requested new id as plain text. The runtime updates the live node, graph edges and input bindings in place.

Response body:

```json
{
  "ok": true,
  "id": "cameraRenamed"
}
```

Status codes:

- `200 OK`: id accepted and renamed
- `409 Conflict`: pipeline is running, source id is missing, or requested id is unavailable

## GET /api/runtime

Returns runtime execution state metadata.

Response fields:

- `state`: `"running" | "stopped"`
- `ui`: `boolean` (`true` when built-in UI mode is active)

Example response:

```json
{
  "state": "running",
  "ui": true
}
```

Status codes:

- `200 OK`

## GET /api/runtime/version

Returns runtime build/version metadata.

Response fields:

- `version`: app version string with leading `v`
- `git`: build git revision string
- `build`: Git commit timestamp in UTC (the field name is retained for compatibility)
- `opencv`: OpenCV version

Example response:

```json
{
  "version": "v0.2.0",
  "git": "ac5b8dd-dirty",
  "build": "2026-01-11 12:34:56 UTC",
  "opencv": "4.12.0"
}
```

Status codes:

- `200 OK`

## PUT /api/runtime

Sets the desired runtime state idempotently.

This endpoint is used by the web supergraph UI for local and remote start/stop control.

Request body:

```json
{
  "desiredState": "stopped"
}
```

Valid `desiredState` values:

- `running`
- `stopped`

Request parsing behavior:

- JSON object with field `desiredState` is supported.
- Plain JSON string values (`"running"`, `"stopped"`) are also accepted.

Response body uses the same schema as `GET /api/runtime`.

Status codes:

- `200 OK`: transition accepted/applied
- `400 Bad Request`: invalid or missing `desiredState`
- `404 Not Found`: runtime controller unavailable

## GET /ws/frame

WebSocket endpoint for live push streaming.

Client request:

- send JSON text frame `subscribe`:

```json
{
  "cmd": "subscribe",
  "nodeId": "v4l2src0",
  "imageKey": "v4l2src0.image"
}
```

Implemented subscribe fields:

- `cmd`: must be `"subscribe"`
- `nodeId`: node filter (optional, defaults to server source node)
- `imageKey`: preferred image key (optional)

Server response per processed frame event:

- one JSON text message with frame-context metadata
- followed by one binary frame packet containing metadata header + raw payload

Frame-context JSON fields:

- `nodeId`: `string`
- `images`: array of image descriptors
  - `key`: `string` (qualified context key)
  - `width`: `number`
  - `height`: `number`
  - `formatId`: `number`
  - `bitsPerPixel`: `number`
  - `sequence`: `number`
  - `timestampNs`: `number`
- `values`: array of scalar descriptors
  - `key`: `string`
  - `type`: `"bool" | "int" | "double" | "string" | "unsupported"`
  - `value`: `string`
- `keys`: `string[]` (all context keys)

Binary frame packet fields:

- packet header (48 bytes, little-endian):
  - bytes `0..3`: magic `0x31464D43` (`CMF1`)
  - bytes `4..5`: version (`1`)
  - bytes `8..11`: width
  - bytes `12..15`: height
  - bytes `16..19`: stride
  - bytes `20..23`: pixel format id
  - bytes `24..25`: bits per pixel
  - bytes `26..27`: bit shift
  - bytes `28..35`: sequence
  - bytes `36..43`: timestamp in ns
  - bytes `44..47`: payload size in bytes
  - bytes `48..`: raw image payload

The JSON context message and its binary image are emitted in order on the same websocket to keep sequence matching deterministic.

## GET /ws/logs

WebSocket endpoint for live application, runtime, node, REST API and kernel log streaming.

Each connected client receives the current log history first and then live log records as JSON text messages.
The stream is always unfiltered; `-L`/`--log-source` affects only process console output.

Record fields:

- `source`: `application`, `kernel`, `runtime`, `node` or `api`
- `type`: `debug`, `info`, `warning` or `error`
- `file`: source file or `kernel`
- `line`: source line number, or `0` for kernel records
- `message`: raw message text
- `rendered`: console-formatted line matching the verbose terminal output
- `timestampMs`: wall-clock time in milliseconds since the Unix epoch

The stream is intended for UI subscriptions and does not require a client subscribe command.

## GET /api/nodes/{id}/parameters

Returns the live parameter schema and current parameter values for a node instance.

Notes:

- Response is separated into:
  - `parameters`: node-owned runtime settings
  - `inputs`: bindable node inputs
  - `outputs`: provided node outputs
- Input bindings are configured in pipeline expressions through parameters using:
  - `<input>=<nodeId>.<output>`
  - Example: `debayer(image=cam0.image)`
- For runtime-discovered nodes such as `v4l2src`, the live schema includes currently available controls.

Example response:

```json
{
  "parameters": [
    {
      "name": "shift",
      "type": "int",
      "description": "Right-shift applied before conversion",
      "default": 0,
      "min": 0,
      "max": 8,
      "runtimeWritable": true,
      "hasSideEffects": false,
      "configured": false,
      "value": 0,
      "origin": "v4l2",
      "source": "/dev/video0"
    }
  ],
  "inputs": [
    {
      "name": "image",
      "type": "image",
      "description": "Input image",
      "allowMultipleBindings": false
    }
  ],
  "outputs": [
    {
      "name": "image",
      "type": "image",
      "description": "Captured frame"
    }
  ]
}
```

Implemented `parameters[]` fields:

- `name`: `string`
- `type`: `"int" | "double" | "bool" | "string" | "option" | "button"`
- `description`: `string`
- `default`: scalar (`bool | number | string`)
- `min`: scalar (`bool | number | string`)
- `max`: scalar (`bool | number | string`)
- `runtimeWritable`: `boolean`
- `hasSideEffects`: `boolean`
- `configured`: `boolean`
- `value`: scalar (`bool | number | string`)
- `options`: `string[]` (optional)
- `optionLabels`: `string[]` (optional)
- `origin`: `string` (optional)
- `source`: `string` (optional)

Implemented `inputs[]` fields:

- `name`: `string`
- `type`: `string`
- `description`: `string`
- `allowMultipleBindings`: `boolean`

Implemented `outputs[]` fields:

- `name`: `string`
- `type`: `string`
- `description`: `string`

Status codes:

- `200 OK`: schema returned
- `404 Not Found`: node not found

## PUT /api/nodes/{id}/parameters/{name}

Also accepts `POST /api/nodes/{id}/parameters/{name}`.

Request:

Plain text request body with the new value, for example:

```text
10000
```

The updated value is applied to the node runtime parameter set.

Response body:

```json
{
  "ok": true
}
```

or

```json
{
  "ok": false,
  "error": "parameter is not writable while runtime is running"
}
```

Status codes:

- `200 OK`: parameter updated
- `404 Not Found`: node or parameter not found
- `409 Conflict`: parameter is not runtime-writable while the runtime is running
- `422 Unprocessable Content`: parameter value was rejected by the node; `error`
  contains the node or driver rejection reason when available

After V4L2 capture starts, CamFlow queries the controls again. Controls reported
as read-only, layout-modifying, grabbed, or inactive are exposed with
`runtimeWritable: false`. A write rejected directly by the V4L2 driver returns
the control name, kernel error text, and `errno` in the `422` response.

Every parameter update attempt is written to the API log with node id, parameter
name and requested value. A second log entry records success or the runtime error,
independently of the general WebServer request verbosity.

Parameters may also advertise extra metadata fields in the JSON schema:

- `optionLabels`: human-friendly labels for option values
- `origin`: optional origin tag such as `v4l2`
- `source`: source device or subsystem identifier
- `hasSideEffects`: indicates that changing the parameter requires a full parameter refresh

## Error responses

Common errors:

- `400 Bad Request`: invalid JSON or invalid parameter.
- `404 Not Found`: node or parameter does not exist.
- `409 Conflict`: graph/node/edge operation rejected.
- `500 Internal Server Error`: runtime failure.
