# Driver Release Test Automation (Central Use Case)

This document defines a central CamFlow use case: automated, reproducible camera-driver release validation with dynamic pipelines, parameter sweeps, and FrameContext-based result logging.

## 1. JSON proposal first (requested)

The following JSON structure models the full scenario and can be consumed by a future experiment runner.

A runnable template variant is also provided in `docs/examples/driver_release_test_plan.example.json`.

```json
{
  "planVersion": "1.0",
  "id": "driver-release-validation",
  "title": "Dual-CSI camera driver release validation",
  "policy": {
    "concreteBenchConfigsInRepo": false,
    "note": "Real serial numbers, local mount details, and operator-specific data stay outside the repository."
  },
  "bench": {
    "platform": "Variscite DART-MX8M-PLUS Evaluation Kit",
    "ports": ["CSI0", "CSI1"],
    "currentBenchCameras": ["IMX900", "IMX900"],
    "releaseCameraTypes": ["IMX273", "IMX415", "IMX540", "IMX566", "IMX900"]
  },
  "dimensions": {
    "cameraType": ["IMX273", "IMX415", "IMX540", "IMX566", "IMX900"],
    "port": ["CSI0", "CSI1"],
    "backend": ["ISI", "ISP"],
    "lanes": [2, 4],
    "pixelFormat": ["RGGB", "RG10", "RG12"],
    "triggerMode": [
      { "name": "freerun", "value": 0 },
      { "name": "self", "value": 4 }
    ]
  },
  "matrix": {
    "formula": "5 cameras * 2 ports * 2 backends * 2 lanes * 3 pixelFormats * 2 triggerModes",
    "expectedRunCount": 240
  },
  "tests": [
    {
      "id": "T01_FIRST10_VALID_STABLE",
      "description": "First 10 frames must be valid and identical according to validator logic.",
      "pipelineTemplate": {
        "nodes": [
          {
            "id": "v4l2src0",
            "type": "v4l2src",
            "parameters": {
              "device": "${port.device}",
              "subdevices": "${port.subdevices.all}",
              "sensorModel": "${cameraType}",
              "lanes": "${lanes}",
              "pixelformat": "${pixelFormat}",
              "triggerMode": "${triggerMode.value}",
              "backend": "${backend}"
            }
          },
          {
            "id": "framevalidate0",
            "type": "framevalidate",
            "parameters": {
              "frameCount": 10,
              "requireEqual": true,
              "saveErrorFrame": true,
              "errorFramePath": "${artifactsDir}/${runId}/error_frame.png"
            }
          }
        ],
        "edges": [
          { "from": "v4l2src0.image", "to": "framevalidate0.image" }
        ]
      },
      "measurement": {
        "warmupFrames": 5,
        "measureFrames": 10,
        "frameContextKeys": [
          "framevalidate0.valid",
          "framevalidate0.equal",
          "framevalidate0.errorCount",
          "v4l2src0.sequence",
          "v4l2src0.timestampNs"
        ],
        "passCriteria": "framevalidate0.valid=true AND framevalidate0.equal=true AND framevalidate0.errorCount=0"
      }
    },
    {
      "id": "T02_MAX_FPS_1MS",
      "description": "Measure maximum FPS with exposure time fixed to 1 ms.",
      "pipelineTemplate": {
        "nodes": [
          {
            "id": "v4l2src0",
            "type": "v4l2src",
            "parameters": {
              "device": "${port.device}",
              "subdevices": "${port.subdevices.all}",
              "sensorModel": "${cameraType}",
              "lanes": "${lanes}",
              "pixelformat": "${pixelFormat}",
              "triggerMode": "${triggerMode.value}",
              "backend": "${backend}",
              "exposureTimeUs": 1000
            }
          }
        ],
        "edges": []
      },
      "measurement": {
        "warmupFrames": 30,
        "measureFrames": 300,
        "frameContextKeys": [
          "v4l2src0.sequence",
          "v4l2src0.timestampNs"
        ],
        "derivedMetrics": ["fps.avg", "fps.max", "fps.p95"],
        "passCriteria": "fps.avg >= ${limits.${cameraType}.${backend}.${lanes}.${pixelFormat}.${triggerMode.name}.minAvgFps}"
      }
    }
  ],
  "execution": {
    "order": ["cameraType", "backend", "port", "lanes", "pixelFormat", "triggerMode", "testId"],
    "stopRuntimeBeforePipelineChange": true,
    "startRuntimeBeforeMeasurement": true,
    "retry": {
      "maxAttempts": 2,
      "cooldownMs": 2000
    }
  },
  "results": {
    "artifactsDir": "${OUT_DIR}/driver-release",
    "writeFormats": ["ndjson", "csv", "json-summary"],
    "record": {
      "fields": [
        "runId",
        "cameraType",
        "port",
        "backend",
        "lanes",
        "pixelFormat",
        "triggerMode",
        "testId",
        "status",
        "metrics",
        "frameContext",
        "artifactPaths",
        "runtimeVersion"
      ]
    },
    "confluence": {
      "tables": [
        {
          "id": "freerun",
          "filter": "triggerMode.name == 'freerun'",
          "groupBy": ["cameraType", "backend", "port", "lanes", "pixelFormat"],
          "columns": ["T01.status", "T01.errorCount", "T02.fps.avg", "T02.fps.p95"]
        },
        {
          "id": "self",
          "filter": "triggerMode.name == 'self'",
          "groupBy": ["cameraType", "backend", "port", "lanes", "pixelFormat"],
          "columns": ["T01.status", "T01.errorCount", "T02.fps.avg", "T02.fps.p95"]
        }
      ]
    }
  }
}
```

## 2. Context and goals

Driver release validation must support frequent camera and configuration changes without hard-coding concrete bench runs in this repository.

### Bench context

- Hardware: Variscite DART-MX8M-PLUS Evaluation Kit
- Camera ports: CSI0 and CSI1
- Current attached cameras: 2x VC MIPI Sony IMX900
- Release camera scope: IMX273, IMX415, IMX540, IMX566, IMX900

### Release workflow model

- For each camera type, connect two equal cameras to CSI0/CSI1.
- Execute the full automated matrix.
- Switch to the next camera type and repeat.

## 3. Test definitions

### T01: First 10 frames valid and stable

Pipeline requirement:

- `v4l2src -> framevalidate`

Expected behavior:

- Validate first 10 frames.
- Detect whether frames are valid and identical according to validator criteria.
- Persist error image when a defect frame is detected.

### T02: Max FPS at 1 ms exposure

Pipeline requirement:

- `v4l2src`

Expected behavior:

- Fixed exposure time: 1 ms.
- Measure max/avg FPS over configured measurement window.

## 4. Matrix and run count

Dimensions:

- lanes: 2, 4
- pixel format: RGGB, RG10, RG12
- trigger mode: freerun (0), self (4)
- ports: CSI0, CSI1
- backend: ISI, ISP
- camera types: IMX273, IMX415, IMX540, IMX566, IMX900

Run count derivation:

- `2 lanes * 3 pixelFormats = 6`
- `6 * 2 triggerModes = 12` per port
- `12 * 2 ports = 24`
- `24 * 2 backends = 48` per camera type
- `48 * 5 camera types = 240`

## 5. Logging and Confluence output

Required output:

- Full machine-readable run log (NDJSON/CSV/JSON summary)
- Compact Confluence export with exactly two tables:
  - freerun table
  - self table

Each row should identify camera, backend, port, lanes, pixel format and include:

- T01 pass/fail and validator error count
- T02 FPS metrics (avg, p95, max)

## 6. Functional requirements derived from this use case

### R1: Dynamic pipeline and parameter automation

- Pipelines must be replaceable dynamically per run.
- Node parameters must be set programmatically per matrix combination.
- Results must be retrievable from FrameContext and persisted by run ID.

### R2: v4l2src subdevice multi-selection

The `subdevices` parameter accepts a comma-separated selection for this use case.

Requirement:

- Support selecting multiple subdevices so CSI and camera subdevices can be controlled together.
- Enable control access to relevant V4L2 controls on all selected subdevices.

### R3: New frame validation processor

A dedicated processor is required for T01:

- Node name proposal: `framevalidate`
- Input: `image`
- Key parameters:
  - `frameCount` (default 10)
  - `requireEqual` (bool)
  - `saveErrorFrame` (bool)
  - `errorFramePath` (string)
- FrameContext outputs proposal:
  - `valid` (bool)
  - `equal` (bool)
  - `errorCount` (int)
  - optional diagnostic values (`diffScore`, `firstErrorIndex`)

## 7. Repository boundary

This document defines automation requirements and a portable JSON model.

- Concrete, operator-specific release campaigns are not stored as fixed repository assets.
- The repository may include only generic templates/examples for tooling integration.
