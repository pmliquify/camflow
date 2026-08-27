# FrameContext

`FrameContext` is the data carrier that moves through a pipeline. It stores typed values in a scoped key namespace.

## Zero-copy policy

Runtime processing uses a zero-copy strategy for context transport:

- Context values are shared by reference between pipeline stages.
- Values are not deep-copied when a context is forwarded, branched or merged.
- A new value instance is created only when a key is explicitly overwritten.

For image payloads this means:

- The runtime avoids image-data copies when the source memory can be referenced safely.
- V4L2 capture uses MMAP-backed buffers and forwards image payloads through shared references.
- Sources that receive transient external buffers may still perform one ingest copy when required by backend lifetime rules.

## Scoped keys

FrameContext stores values in separate scopes. Nodes write logical keys such as `image`.
The runtime stores unqualified keys as `<nodeId>.<key>`.

Input routing is configured by node input bindings:

- `<input>=<nodeId>.<output>`

Example:

- `image=cam0.image`
- `image=cam0.image,cam1.image`

Node implementations resolve these bindings and read values from the referenced
scope/key pairs in `FrameContext`.

Examples:

- `cam0.image`
- `debayer0.image`
- `join.image`

This prevents key conflicts when branches merge.

## Default image key

The default logical image key remains `image`. The earlier `image.raw` convention is not used for the default runtime flow.

## Branching

When a pipeline branch splits, each branch receives an independent `FrameContext` view with shared value references.

```text
v4l2src -> [filesink, tcpsink]
```

Both sinks receive the same initial image reference (`v4l2src0.image`). If one branch writes `image`, that branch writes into its own scoped key.

## Merging

When branches merge, all scoped entries are preserved under their full keys. The old conflict-suffix naming (`image#1`, `image.1`, ...) is no longer used.

If the same fully-qualified key appears twice, the merged-in value overwrites the previous value.

Example:

```text
cam0.image
cam1.image
```

A compositor processor such as `CompositorProcessor` can consume multiple scoped inputs and write a new logical `image` result.

## Scope-aware reads

Unqualified reads like `get("image")` resolve in this order:

- the stored scope order, searched backwards so the latest matching key wins

Fully qualified reads like `get("cam1.image")` are always explicit and only
match that exact scope.

## Required vs optional reads

- `get<T>(...)` without default is required. If the key is missing, an error is logged and `nullptr` is returned.
- `get<T>(..., defaultValue)` is optional. If the key is missing, no error is logged and the default is returned.
- Type mismatches are still logged for both required and optional reads.

## Default-value getters

FrameContext supports default-value reads to avoid pointer checks in nodes:

- `context.get<int64_t>("cam0", "xpos", 0)`
- `context.get<bool>("myNode", "enabled", false)`

If the key is missing, the provided default is returned without an error log.

## Streaming and UI mode

`TCPSink` sends image payloads through the ImageSocket protocol. In UI mode, the embedded web service provides browser preview as JPEG.

## Future typed serialization

Planned typed entries:

- `ImageBuffer`
- scalar values
- strings
- arrays
- JSON-compatible result structures

## Supergraph transport and retention

For multi-runtime supergraphs, FrameContext values are transported transparently by the runtime for cross-runtime node edges.

- Users define regular node-to-node edges, even when source and target nodes are on different runtimes.
- The scheduler keeps this node-level edge model and performs network transport internally.
- Producer scope data required by the target side is forwarded automatically.
- Multiple parallel cross-runtime edges are supported.

The runtime now also applies graph-based retention:

- Before and during execution, the scheduler tracks how many downstream consumers still require each producer scope.
- After the last consumer of a scope has executed, that scope is removed from FrameContext.

This keeps memory usage bounded for large supergraphs while preserving existing node behavior.
