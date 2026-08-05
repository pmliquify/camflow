# JSON Graph Format

Graphs can be loaded through `--graph` (short form `-G`) or through the REST API.

## Minimal UI graph

```json
{
  "nodes": [
    { "id": "cam", "type": "v4l2src", "parameters": { "device": "/dev/video0", "pixelformat": "RG10" } },
    { "id": "stream", "type": "tcpsink", "parameters": { "ip": "192.168.1.100", "port": 9000 } }
  ],
  "edges": [
    { "from": "cam", "to": "stream" }
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
    { "from": "cam", "to": "stream" }
  ]
}
```

## Split stream graph

```json
{
  "nodes": [
    { "id": "cam", "type": "v4l2src" },
    { "id": "store", "type": "filesink", "parameters": { "directory": "out", "appendSequence": true, "appendTimestamp": true } },
    { "id": "stream", "type": "tcpsink", "parameters": { "ip": "192.168.1.100", "port": 9000 } }
  ],
  "edges": [
    { "from": "cam", "to": "store" },
    { "from": "cam", "to": "stream" }
  ]
}
```

## Two-source merge graph

```json
{
  "nodes": [
    { "id": "left", "type": "filesrc", "parameters": { "file": "left.png" } },
    { "id": "right", "type": "filesrc", "parameters": { "file": "right.png" } },
    { "id": "join", "type": "compositor" },
    { "id": "out", "type": "filesink", "parameters": { "file": "joined.png" } }
  ],
  "edges": [
    { "from": "left", "to": "join" },
    { "from": "right", "to": "join" },
    { "from": "join", "to": "out" }
  ]
}
```
