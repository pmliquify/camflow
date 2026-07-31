export async function getRuntimeStatus(baseUrl = '') {
        const response = await fetch(`${baseUrl}/api/runtime`, { cache: 'no-store' });
        if (!response.ok) {
                throw new Error(`runtime status ${response.status}`);
        }
        return response.json();
}

export async function setRuntimeStopped(stopped, baseUrl = '') {
        const response = await fetch(`${baseUrl}/api/runtime`, {
                method: 'PUT',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ desiredState: stopped ? 'stopped' : 'running' })
        });
        if (!response.ok) {
                throw new Error(`runtime toggle ${response.status}`);
        }
        return response.json();
}

export async function getRuntimeVersion() {
        const response = await fetch('/api/runtime/version', { cache: 'no-store' });
        if (!response.ok) {
                throw new Error(`runtime version ${response.status}`);
        }
        return response.json();
}

export async function getNodeCatalog() {
        const response = await fetch('/api/nodes', { cache: 'no-store' });
        if (!response.ok) {
                throw new Error(`node catalog ${response.status}`);
        }
        return response.json();
}

export async function getPipeline() {
        const response = await fetch(`/api/pipeline?t=${Date.now()}`, { cache: 'no-store' });
        if (!response.ok) {
                throw new Error(`pipeline ${response.status}`);
        }
        return response.json();
}

export async function savePipeline(payload) {
        const response = await fetch('/api/pipeline', {
                method: 'PUT',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(payload)
        });
        if (!response.ok) {
                throw new Error(`save pipeline ${response.status}`);
        }
        return response;
}

export async function addNode(payload) {
        const response = await fetch('/api/nodes', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(payload)
        });
        if (!response.ok) {
                const detail = await response.text().catch(() => '');
                throw new Error(detail ? `add node ${response.status}: ${detail}` : `add node ${response.status}`);
        }
        return response.json().catch(() => ({}));
}

export async function deleteNode(nodeId) {
        const response = await fetch('/api/nodes/' + encodeURIComponent(nodeId), {
                method: 'DELETE'
        });
        if (!response.ok) {
                throw new Error(`delete node ${response.status}`);
        }
        return response;
}

export async function renameNode(nodeId, newNodeId) {
        const response = await fetch('/api/nodes/' + encodeURIComponent(nodeId) + '/id', {
                method: 'PUT',
                body: String(newNodeId)
        });
        if (!response.ok) {
                throw new Error(`rename node ${response.status}`);
        }
        return response;
}

function parseEdgeExpression(edgeText) {
        const raw = String(edgeText || '');
        const arrowPos = raw.indexOf('->');
        if (arrowPos < 0) {
                return null;
        }

        const left = raw.slice(0, arrowPos).trim();
        const right = raw.slice(arrowPos + 2).trim();
        if (!left || !right) {
                return null;
        }

        const parseEndpoint = (endpoint) => {
                const dot = endpoint.lastIndexOf('.');
                if (dot <= 0 || dot + 1 >= endpoint.length) {
                        return { node: endpoint.trim(), port: '' };
                }
                return {
                        node: endpoint.slice(0, dot).trim(),
                        port: endpoint.slice(dot + 1).trim()
                };
        };

        const from = parseEndpoint(left);
        const to = parseEndpoint(right);
        if (!from.node || !to.node) {
                return null;
        }

        return {
                fromNode: from.node,
                fromPort: from.port || 'image',
                toNode: to.node,
                toPort: to.port || 'image'
        };
}

function normalizeEdgePayload(edgeInput) {
        if (edgeInput && typeof edgeInput === 'object') {
                return {
                        fromNode: String(edgeInput.fromNode || '').trim(),
                        fromPort: String(edgeInput.fromPort || '').trim() || 'image',
                        toNode: String(edgeInput.toNode || '').trim(),
                        toPort: String(edgeInput.toPort || '').trim() || 'image'
                };
        }

        const parsed = parseEdgeExpression(edgeInput);
        if (!parsed) {
                throw new Error('invalid edge expression');
        }
        return parsed;
}

export async function createEdge(edgeInput) {
        const payload = normalizeEdgePayload(edgeInput);
        const response = await fetch('/api/edges', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(payload)
        });
        if (!response.ok) {
                throw new Error(`create edge ${response.status}`);
        }
        return response;
}

export async function deleteEdge(edgeInput) {
        const payload = normalizeEdgePayload(edgeInput);
        const response = await fetch('/api/edges', {
                method: 'DELETE',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(payload)
        });
        if (!response.ok) {
                throw new Error(`delete edge ${response.status}`);
        }
        return response;
}

export async function getNodeParameters(nodeId) {
        const response = await fetch('/api/nodes/' + encodeURIComponent(nodeId) + '/parameters', { cache: 'no-store' });
        if (!response.ok) {
                throw new Error(`node params ${response.status}`);
        }
        return response.json();
}

export async function updateNodeParameter(nodeId, name, value) {
        const response = await fetch('/api/nodes/' + encodeURIComponent(nodeId) + '/parameters/' + encodeURIComponent(name), {
                method: 'PUT',
                body: String(value)
        });
        if (!response.ok) {
                throw new Error(`update param ${response.status}`);
        }
        return response;
}
