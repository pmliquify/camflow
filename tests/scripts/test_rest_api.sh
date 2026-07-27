#!/usr/bin/env bash
set -euo pipefail

HOST="${1:-127.0.0.1}"
PORT="${2:-8080}"
BASE="http://${HOST}:${PORT}/api"

echo "GET /api/nodes"
curl -fsS "${BASE}/nodes" | sed 's/.*/&\n/'

echo "GET /api/pipeline"
curl -fsS "${BASE}/pipeline" | sed 's/.*/&\n/'

echo "GET /api/nodes/left/parameters"
curl -fsS "${BASE}/nodes/left/parameters" | sed 's/.*/&\n/'

echo "GET /api/nodes/output/parameters"
curl -fsS "${BASE}/nodes/output/parameters" | sed 's/.*/&\n/'

echo "PUT /api/nodes/output/parameters/appendSequence"
curl -fsS -X PUT -d true "${BASE}/nodes/output/parameters/appendSequence" | sed 's/.*/&\n/'

echo "GET /api/nodes/output/parameters after update"
curl -fsS "${BASE}/nodes/output/parameters" | sed 's/.*/&\n/'
