# JSON Graph Format

Graphs can be loaded through `--graph` (short form `-G`) or through the REST API
(`GET`/`PUT`/`POST /api/pipeline`).

The JSON graph document is parsed with a small, purpose-built reader, not a
general JSON library. Keep to the shapes documented below.

## Top-level structure

```json
{
  "nodes": [
    { "id": "<nodeId>", "type": "<nodeType>", "parameters": { "<name>": "<value>" } }
  ],
  "edges": [
    "<fromId>[.<fromPort>] -> <toId>[.<toPort>]"
  ]
}
```

- `nodes` is required. Each node object's fields must appear in this exact
  order: `id`, `type`, then an optional `parameters` object.
- `parameters` values must be scalars: a quoted string, `true`/`false`, or a
  number (integer or decimal). Arrays and nested objects are not supported;
  use a comma-separated string instead (as in the CLI syntax), for example
  `"subdevices": "/dev/v4l-subdev0,/dev/v4l-subdev1"`.
- `edges` is optional and is an array of plain arrow-syntax strings, not
  objects. Each string has the form `fromId[.fromPort] -> toId[.toPort]`.
  Omitted ports default to `output` on the source side and `input` on the
  target side.

## Minimal UI graph

```json
{
  "nodes": [
    { "id": "cam", "type": "v4l2src", "parameters": { "device": "/dev/video0", "pixelformat": "RG10" } },
    { "id": "stream", "type": "tcpsink", "parameters": { "ip": "192.168.1.100", "port": 9000 } }
  ],
  "edges": [
    "cam -> stream"
  ]
}
```

## Raw bit-shift metadata graph

```json
{
  "nodes": [
    { "id": "cam", "type": "v4l2src", "parameters": { "bitShift": 2 } },
    { "id": "stream", "type": "tcpsink", "parameters": { "ip": "192.168.1.100", "port": 9000 } }
  ],
  "edges": [
    "cam -> stream"
  ]
}
```

## Split stream graph

```json
{
  "nodes": [
    { "id": "cam", "type": "v4l2src" },
    { "id": "store", "type": "filesink", "parameters": { "format": "raw", "filename": "out/frame", "appendSequence": true, "appendDatetime": true } },
    { "id": "stream", "type": "tcpsink", "parameters": { "ip": "192.168.1.100", "port": 9000 } }
  ],
  "edges": [
    "cam -> store",
    "cam -> stream"
  ]
}
```

## Debayer processing graph

```json
{
  "nodes": [
    { "id": "cam", "type": "filesrc", "parameters": { "filename": "tests/images/frame_*.raw" } },
    { "id": "deb", "type": "debayer" },
    { "id": "out", "type": "filesink", "parameters": { "format": "png", "filename": "out/color" } }
  ],
  "edges": [
    "cam -> deb",
    "deb -> out"
  ]
}
```

## Explicit input bindings

Instead of (or in addition to) plain `edges`, a node input can be bound
explicitly through its `parameters`, using the same
`<input>=<nodeId>.<output>` syntax as the CLI DSL. This is required whenever
an input must reference a specific upstream output key rather than being
auto-resolved from graph connectivity, and it is the only way to bind
multiple sources to a single multi-binding input:

```json
{ "id": "join", "type": "compositor", "parameters": { "image": "left.image,right.image" } }
```

`compositor` and `ccm` are optional nodes disabled by default
(`ENABLE_PROCESSOR_COMPOSITOR`, `ENABLE_PROCESSOR_CCM`) and are not part of
the standard release build; see [Nodes](nodes.md).

## Notes

- `GET /api/pipeline` returns the graph in this same shape, with `edges`
  serialized as `fromId.fromPort -> toId.toPort` strings.
- Node type names are normalized to lowercase, matching the CLI syntax.
