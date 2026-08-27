# Scheduler

The pipeline is represented as a directed acyclic graph. A node is executable when all required input contexts are available.

## Current execution model

The runtime contains a graph-based execution model that supports branches and merge points. This version keeps the implementation simple but documents the intended scheduler semantics.

## Parallel execution requirement

If the graph allows parallel execution, independent branches shall be executed in separate worker threads.

Example:

```text
v4l2src -> [filesink, tcpsink]
```

`filesink` and `tcpsink` can run in parallel because neither depends on the other.

## Merge synchronization

A merge node waits until all incoming branches provide a compatible context. Sequence metadata is used to decide whether input data belongs to the same frame group.

## Design target

The scheduler should eventually use:

- topological sorting;
- a worker thread pool;
- per-edge context queues;
- bounded queues for back pressure;
- explicit cancellation and shutdown;
- deterministic error propagation.
