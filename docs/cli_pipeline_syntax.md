# CLI Pipeline Syntax

This document defines the compact CLI syntax used by CamFlow to describe runtime processing graphs.

## Overview

A pipeline is a directed flow of nodes connected by arrows:

```text
source -> processor -> sink
```

Node categories:

- Source
- Processor
- Sink

## Node Declaration

A node has this form:

```text
<id>:<type>(parameter...)
```

Examples:

```text
cam:v4l2src(device=/dev/video0)
v4l2src(device=/dev/video0)
compositor{cam0,cam1}(cam0.xpos=0,cam1.xpos=320)
compositor(image=cam0.image,cam1.image,xpos=0,320)
```

Rules:

- The id is optional.
- If no id is provided, an automatic id is generated from the node type and an index.
- Node type names are normalized internally to lowercase and without separators.

## Parameters

Parameters are declared inside parentheses.

```text
filesink(path=images,format=PNG)
```

Parameter values support:

- string values (`path=/tmp/output`)
- quoted strings (`name="camera 0"`)
- booleans (`enabled=true`)
- integers (`port=9000`)
- floating point values (`gain=1.5`)
- flag-style parameters (`debug`), interpreted as `true`

Option parameters are represented as plain string values in CLI expressions.

For `v4l2src`, `device` and `subdevices` are runtime options discovered from the local system. In CLI you still pass them as strings:

```text
v4l2src(device=/dev/video0,subdevices=/dev/v4l-subdev0,/dev/v4l-subdev1)
```

Input bindings are configured as parameter assignments:

- `<input>=<nodeId>.<output>`
- example: `image=cam0.image`
- multi-binding input example: `image=cam0.image,cam1.image`

Values after a parameter can continue across commas to support lists:

- `xpos=0,100`
- `ypos=0,0`

## Linear Flow

```text
v4l2src -> debayer -> tcpsink
```

## Branching

A branch group is declared with parentheses:

```text
v4l2src -> (filesink, tcpsink)
```

Equivalent expanded form:

```text
v4l2src -> filesink
v4l2src -> tcpsink
```

All branches are independent and can run in parallel.

## Nested Branching

Branch groups can be nested:

```text
v4l2src -> (filesink, debayer -> (histogram, threshold))
```

## Merge Behavior

If a branch group is followed by another arrow, all open branch endpoints are merged into the next node.

```text
debayer -> (histogram, compositor, threshold) -> classifier
```

Equivalent dependency shape:

- `histogram -> classifier`
- `compositor -> classifier`
- `threshold -> classifier`

At runtime the merged `FrameContext` keeps all scoped keys (for example `cam0.image`, `cam1.image`) without generating conflict suffixes.

## Sink Semantics

A sink ends a branch.

```text
source -> (filesink, processor) -> tcpsink
```

Equivalent behavior:

- `source -> filesink`
- `source -> processor -> tcpsink`

`filesink` does not continue into `tcpsink`.

## Multiple Sinks in One Branch Group

```text
processor -> (tcpsink, filesink)
```

Both branches terminate at sinks.

## Multiple Top-Level Pipelines

Multiple independent pipelines can be written in one expression using a top-level comma:

```text
v4l2src(device=/dev/video0) -> tcpsink,
v4l2src(device=/dev/video1) -> tcpsink
```

Each top-level pipeline is parsed independently.

## Probe Operator

A probe can be inserted directly in an arrow:

```text
v4l2src -captureA> debayer
```

Equivalent expanded form:

```text
v4l2src -> probe(captureA) -> debayer
```

Probe ids in the arrow operator are arbitrary non-empty strings and become the inserted probe node id.

The probe is inserted as a node in the graph and does not change data flow topology.

`probe` is a regular registered runtime node type.

It is also valid to end a pipeline directly with a probe:

```text
v4l2src -captureA>
```

This is equivalent to `v4l2src -> probe(captureA)`.

## Automatic IDs

If an id is omitted, the parser creates one automatically:

```text
filesink
```

becomes an internal id such as `filesink0`.

Repeated unnamed nodes of the same type receive increasing indices.

## Explicit IDs

```text
raw:filesink(path=raw)
png:filesink(path=result)
```

Ids must be unique within one parsed expression.

## Complex Example

```text
v4l2src(device=/dev/video0)
-captureA>
(
    raw:filesink(path=raw),
    deb:debayer
    ->(
        histogram,
        compositor{deb},
        undistort->threshold->blobdetector
    )
    -defectCheck>
    defectclassifier
)
->(
    tcpsink,
    png:filesink(path=result)
)
```

## Compositor Example

This example binds two source outputs into the compositor image input:

```text
(cam0:v4l2src(device=/dev/video0) -> debayer,
 cam1:v4l2src(device=/dev/video1) -> debayer)
-> compositor(
    image=cam0.image,cam1.image,
    xpos=0,320,
    ypos=0,120,
    zorder=0,1
)
-> tcpsink
```

`compositor` input behavior:

- required input: `image`
- `image` allows multiple bindings
- position and order arrays are aligned by image index
- missing trailing values reuse the last provided value

## Notes

- The syntax maps naturally to a DAG representation.
- Branches model parallel paths.
- Merge points are implicit and created from downstream connectivity.
- Invalid or unbalanced expressions return parser errors.
