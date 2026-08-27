# Debugging

## Local runtime debugging

Use VS Code with the provided `.vscode/launch.json` and `.vscode/tasks.json`.

## Remote target debugging

On the target:

```bash
gdbserver :1234 /tmp/camflow "v4l2src(device=/dev/video0)->tcpsink(ip=<host-ip>,port=9000)"
```

On the development machine configure VS Code `cppdbg` with:

```json
{
  "name": "camflow remote target",
  "type": "cppdbg",
  "request": "launch",
  "program": "${workspaceFolder}/build-runtime/camflow",
  "miDebuggerServerAddress": "<target-ip>:1234",
  "MIMode": "gdb"
}
```

The runtime can be inspected from the host debugger while the target pipeline is paused at breakpoints.

## Runtime profiling

Enable node-level runtime profiling from the CLI:

```bash
camflow --profile -n 300 "v4l2src(device=/dev/video0)->logsink"
camflow --simple-pipeline --profile -n 300 "v4l2src(device=/dev/video0)->logsink"
```

At shutdown, camflow prints one profiling line per node with calls, failures, and min/avg/max/total execution times.
It also prints a `TOTAL` line across all nodes. Times are shown in milliseconds with 3 decimal places.

## UI mode debugging

```bash
camflow --port 8080
```

Open `http://<target-ip>:8080` and inspect browser network requests for `/api/*` endpoints.
