import React, { useEffect, useLayoutEffect, useMemo, useRef, useState } from 'react';
import GlobalHeader from './features/header/GlobalHeader.jsx';
import NodeEditorPanel from './features/node-editor/NodeEditorPanel.jsx';
import ContextMenu from './features/node-editor/ContextMenu.jsx';
import FrameViewerPanel from './features/frame-context-viewer/FrameViewerPanel.jsx';
import ParameterPanel from './features/parameter-browser/ParameterPanel.jsx';
import Button from './components/Button.jsx';
import Input from './components/Input.jsx';
import useRemoteRuntimeStatus from './hooks/useRemoteRuntimeStatus.js';
import {
        addNode,
        createEdge,
        deleteEdge as deleteEdgeApi,
        deleteNode as deleteNodeApi,
        getNodeCatalog,
        getNodeParameters,
        getPipeline,
        getRuntimeStatus,
        getRuntimeVersion,
        renameNode as renameNodeApi,
        savePipeline,
        setRuntimeStopped,
        updateNodeParameter
} from './services/runtimeApi.js';
import { formatLabel, parseBinaryFramePacket, renderPacketToRgba } from './services/frameRendering.js';

const WS_FRAME = '/ws/frame';
const LOCAL_RUNTIME_ID = 'local';

const FALLBACK_NODE_TYPES = {
        sources: ['v4l2src', 'filesrc', 'nvargussrc'],
        processors: ['debayer', 'ccm', 'compositor'],
        sinks: ['filesink', 'logsink', 'tcpsink']
};

const DEFAULT_MODE = 'viewer';
const VIEW_MODE_STORAGE_KEY = 'camflow:view-mode';
const SELECTED_NODE_STORAGE_KEY = 'camflow:selected-node-id';
const RUNTIME_LAYOUTS_STORAGE_KEY = 'camflow:runtime-layouts';
const NODE_LAYOUTS_STORAGE_KEY = 'camflow:node-layouts';
const NODE_NAMES_STORAGE_KEY = 'camflow:node-names';
const RUNTIME_NAMES_STORAGE_KEY = 'camflow:runtime-names';
const NODE_PORT_VISIBILITY_STORAGE_KEY = 'camflow:node-port-visibility';
const FRAME_VIEWER_SETTINGS_STORAGE_KEY = 'camflow:frame-viewer-settings:v1';
const DEFAULT_FRAME_VIEWER_SETTINGS = { debayerEnabled: false };
const AUTO_NODE_PREFIX = '__auto__';
const NODE_WIDTH = 152;
const NODE_HEIGHT = 62;
const NODE_HEADER_HEIGHT = 28;
const NODE_PORT_ROW_HEIGHT = 28;
const RUNTIME_HEADER_HEIGHT = 25;
const RUNTIME_MIN_WIDTH = 190;
const RUNTIME_MIN_HEIGHT = 120;
const EDITOR_MIN_ZOOM = 0.35;
const EDITOR_MAX_ZOOM = 2.25;
const PAN_DRAG_THRESHOLD = 8;
const DEFAULT_RUNTIME_VIEWPORT = { zoom: 1, panX: 0, panY: 0 };
const DEFAULT_RUNTIME_API_PORT = '8000';
const PARAMETER_TYPEAHEAD_TIMEOUT_MS = 700;

function isAutoNodeId(id) {
        return String(id || '').startsWith(AUTO_NODE_PREFIX);
}

function rectsOverlap(a, b) {
        return a.x < b.x + b.w && a.x + a.w > b.x && a.y < b.y + b.h && a.y + a.h > b.y;
}

function runtimeDefaultRect(index) {
        const col = index % 2;
        const row = Math.floor(index / 2);
        return {
                x: 24 + col * 430,
                y: 24 + row * 300,
                w: 400,
                h: 260
        };
}

function nodeDefaultPos(index) {
        return {
                x: 36 + (index % 3) * 170,
                y: 42 + Math.floor(index / 3) * 86
        };
}

function nextName(prefix, items) {
        const used = new Set((items || []).map((item) => String(item.name || item.id || '').toLowerCase()));
        let index = 1;
        while (used.has(`${prefix}${index}`)) index += 1;
        return `${prefix}${index}`;
}

function autoNodeId() {
        return AUTO_NODE_PREFIX;
}

function wsUrl(path) {
        const explicitTarget = (import.meta.env.VITE_CAMFLOW_WS_TARGET || '').trim();
        if (explicitTarget) {
                return explicitTarget.replace(/\/$/, '') + path;
        }

        const protocol = window.location.protocol === 'https:' ? 'wss://' : 'ws://';
        return protocol + window.location.host + path;
}

function wsUrlFromBase(baseUrl, path) {
        const trimmed = String(baseUrl || '').trim();
        if (!trimmed) {
                return '';
        }
        return trimmed.replace(/^http/i, 'ws').replace(/\/$/, '') + path;
}

function runtimeLogWsUrl(runtime, localIp) {
        if (!runtime) {
                return '';
        }

        const runtimeIp = String(runtime.ip || '').trim();
        if (runtime.id === LOCAL_RUNTIME_ID || runtimeIp === '' || runtimeIp === localIp || runtimeIp === '127.0.0.1' || runtimeIp === 'localhost') {
                return wsUrl('/ws/logs');
        }

        const baseUrl = runtimeBaseUrl(runtimeIp);
        return wsUrlFromBase(baseUrl, '/ws/logs');
}

function runtimeIdFromNode(node, localIp) {
        const params = node?.parameters || {};
        const candidate = params.runtimeTargetIp || params.targetIp || params.runtimeIp || params.target || params.runtime || '';
        const runtimeId = String(candidate || '').trim();
        if (!runtimeId || runtimeId === '127.0.0.1' || runtimeId === 'localhost' || runtimeId === localIp) {
                return LOCAL_RUNTIME_ID;
        }
        return runtimeId;
}

function runtimeDisplayLabel(runtime, localIp) {
        if (!runtime) {
                return '';
        }
        if (runtime.displayName) {
                return String(runtime.displayName);
        }
        if (runtime.id === LOCAL_RUNTIME_ID) {
                const localHost = String(runtime.ip || localIp || 'localhost').trim();
                return localHost === '127.0.0.1' ? 'localhost' : localHost;
        }
        return String(runtime.name || runtime.ip || runtime.id || 'runtime');
}

function isInteractiveHeaderTarget(target) {
        const element = target instanceof Element ? target : null;
        return Boolean(element && element.closest('button,input,select,textarea,a'));
}

function areKeyboardShortcutsBlocked() {
        return Boolean(document.querySelector('.dialog-backdrop'));
}

function focusAdjacentParameter(event) {
        event.preventDefault();
        if (areKeyboardShortcutsBlocked()) {
                return;
        }
        const controls = Array.from(document.querySelectorAll('.parameter input, .parameter select, .parameter button'))
                .filter((element) => !element.disabled && element.getClientRects().length > 0);
        if (controls.length === 0) {
                return;
        }

        const currentIndex = controls.indexOf(document.activeElement);
        const nextIndex = event.shiftKey
                ? (currentIndex <= 0 ? controls.length - 1 : currentIndex - 1)
                : (currentIndex < 0 || currentIndex === controls.length - 1 ? 0 : currentIndex + 1);
        controls[nextIndex].focus();
}

function focusParameterByTypeahead(event, typeaheadRef) {
        if (event.defaultPrevented || event.repeat || event.altKey || event.ctrlKey || event.metaKey || event.key.length !== 1 || !/\p{L}/u.test(event.key)) {
                return false;
        }

        const now = Date.now();
        const current = typeaheadRef.current;
        const continuing = now - current.timestamp <= PARAMETER_TYPEAHEAD_TIMEOUT_MS;
        const target = event.target instanceof Element ? event.target : null;
        const editableTarget = target?.closest('textarea, select, [contenteditable="true"], input:not([type="checkbox"]):not([type="radio"]):not([type="range"]):not([type="button"]):not([type="submit"])');
        if (editableTarget && editableTarget !== current.focusedElement) {
                return false;
        }

        event.preventDefault();
        const searchText = `${continuing ? current.text : ''}${event.key}`.toLocaleLowerCase();
        typeaheadRef.current = { text: searchText, timestamp: now, focusedElement: current.focusedElement };
        const rows = Array.from(document.querySelectorAll('.parameter[data-parameter-name]'))
                .filter((row) => row.getClientRects().length > 0);
        const match = rows.find((row) => {
                const displayName = String(row.dataset.parameterName || '').toLocaleLowerCase();
                const fullName = String(row.dataset.parameterFullName || '').toLocaleLowerCase();
                return displayName.startsWith(searchText) || fullName.startsWith(searchText);
        });
        if (!match) {
                return true;
        }

        const controls = Array.from(match.querySelectorAll('input, select, button'))
                .filter((element) => !element.disabled && element.getClientRects().length > 0 && !element.closest('.param-name'));
        const focusTarget = controls[0] || match;
        focusTarget.focus();
        typeaheadRef.current.focusedElement = focusTarget;
        return true;
}

function splitVersionText(versionText) {
        const text = String(versionText || '').trim();
        if (!text) {
                return { primary: '', secondary: '' };
        }
        const match = text.match(/^(v[^\s|]+)/i);
        if (!match) {
                return { primary: text, secondary: '' };
        }
        const primary = match[1];
        const secondary = text.slice(primary.length).trim();
        return { primary, secondary };
}

function normalizeRuntimeHost(runtimeIp) {
        const raw = String(runtimeIp || '').trim();
        if (!raw) {
                return '';
        }
        if (raw.includes('://')) {
                try {
                        const url = new URL(raw);
                        return url.host;
                } catch (_) {
                        return raw;
                }
        }
        return raw;
}

function runtimeBaseUrl(runtimeIp) {
        const host = normalizeRuntimeHost(runtimeIp);
        if (!host) {
                return '';
        }
        if (host.includes(':')) {
                return `${window.location.protocol}//${host}`;
        }
        return `${window.location.protocol}//${host}:${DEFAULT_RUNTIME_API_PORT}`;
}

function portAnchor(node, portName, direction) {
        const ports = direction === 'output' ? node.visibleOutputs : node.visibleInputs;
        const index = Math.max(0, (ports || []).indexOf(portName));
        const scale = node.scale || 1;
        return {
                x: direction === 'output' ? node.x + NODE_WIDTH * scale : node.x,
                y: node.y + (NODE_HEADER_HEIGHT + index * NODE_PORT_ROW_HEIGHT + NODE_PORT_ROW_HEIGHT * 0.5) * scale
        };
}

function routedEdgeCurvePath(fromNode, toNode, edge) {
        const from = portAnchor(fromNode, edge?.fromPort || 'image', 'output');
        const to = portAnchor(toNode, edge?.toPort || 'image', 'input');
        const x1 = from.x;
        const y1 = from.y;
        const x2 = to.x;
        const y2 = to.y;
        const distance = Math.hypot(x2 - x1, y2 - y1);
        const control = Math.max(34, Math.min(190, distance * 0.38));
        return `M ${x1} ${y1} C ${x1 + control} ${y1}, ${x2 - control} ${y2}, ${x2} ${y2}`;
}

function absoluteEdgeCurvePath(fromNode, toNode, edge) {
        return routedEdgeCurvePath(fromNode, toNode, edge);
}

function nodeKind(type, catalog) {
        return ['sources', 'processors', 'sinks'].find((kind) => (catalog?.[kind] || []).includes(type)) || 'processors';
}

function nodeSchema(type, catalog) {
        const schema = catalog?.schemas?.[type];
        if (schema) {
                return schema;
        }
        const kind = nodeKind(type, catalog);
        return {
                inputs: kind === 'sources' ? [] : [{ name: 'image', type: 'image' }],
                outputs: kind === 'sinks' ? [] : [{ name: 'image', type: 'image' }]
        };
}

function buildEditorGraph(pipeline, localDraftRuntimes, localIp, runtimeLayouts, nodeLayouts, runtimeStatus, remoteRuntimeStatuses, catalog, nodeNames, runtimeNames, nodePortVisibility) {
        const runtimeMap = new Map();
        runtimeMap.set(LOCAL_RUNTIME_ID, { id: LOCAL_RUNTIME_ID, name: 'runtime local', ip: localIp, nodes: [], edges: [] });
        const nodeRuntime = new Map();
        const liveNodeIds = new Set((Array.isArray(pipeline?.nodes) ? pipeline.nodes : []).map((node) => node.id));
        const draftRuntimeIds = new Set((Array.isArray(localDraftRuntimes) ? localDraftRuntimes : []).map((runtime) => runtime.id));

        const ensureRuntime = (runtimeId, name, ip) => {
                if (!runtimeMap.has(runtimeId)) {
                        runtimeMap.set(runtimeId, { id: runtimeId, name, ip, nodes: [], edges: [] });
                }
                return runtimeMap.get(runtimeId);
        };

        (Array.isArray(pipeline?.nodes) ? pipeline.nodes : []).forEach((node, index) => {
                if (isAutoNodeId(node.id)) {
                        return;
                }
                const runtimeId = runtimeIdFromNode(node, localIp);
                const runtime = ensureRuntime(runtimeId, runtimeId === LOCAL_RUNTIME_ID ? 'runtime local' : `runtime ${runtimeId}`, runtimeId === LOCAL_RUNTIME_ID ? localIp : runtimeId);
                nodeRuntime.set(node.id, runtimeId);
                const defaultPos = nodeDefaultPos(index);
                const layoutPos = nodeLayouts?.[node.id];
                const type = node.type || 'node';
                const schema = nodeSchema(type, catalog);
                runtime.nodes.push({ id: node.id, name: nodeNames?.[node.id] || node.name, type, kind: nodeKind(type, catalog), inputs: schema.inputs || [], outputs: schema.outputs || [], x: layoutPos?.x ?? defaultPos.x, y: layoutPos?.y ?? defaultPos.y, live: true });
        });

        (Array.isArray(localDraftRuntimes) ? localDraftRuntimes : []).forEach((runtime) => {
                const target = ensureRuntime(runtime.id, runtime.name, runtime.ip);
                (runtime.nodes || []).forEach((node, index) => {
                        if (isAutoNodeId(node.id) || liveNodeIds.has(node.id)) {
                                return;
                        }
                        const defaultPos = nodeDefaultPos(index);
                        const layoutPos = nodeLayouts?.[node.id];
                        const schema = nodeSchema(node.type, catalog);
                        target.nodes.push({ ...node, name: nodeNames?.[node.id] || node.name, kind: nodeKind(node.type, catalog), inputs: schema.inputs || [], outputs: schema.outputs || [], x: layoutPos?.x ?? node.x ?? defaultPos.x, y: layoutPos?.y ?? node.y ?? defaultPos.y });
                });
                (runtime.edges || []).forEach((edge) => target.edges.push(edge));
        });

        const nodesById = new Map();
        runtimeMap.forEach((runtime) => runtime.nodes.forEach((node) => nodesById.set(node.id, node)));

        const crossRuntimeEdges = [];
        (Array.isArray(pipeline?.edges) ? pipeline.edges : []).forEach((rawEdge, index) => {
                if (typeof rawEdge !== 'string') return;
                const arrowPos = rawEdge.indexOf('->');
                if (arrowPos < 0) return;
                const left = rawEdge.slice(0, arrowPos).trim();
                const right = rawEdge.slice(arrowPos + 2).trim();
                const leftDot = left.lastIndexOf('.');
                const rightDot = right.lastIndexOf('.');
                if (leftDot <= 0 || rightDot <= 0) return;
                const fromNode = left.slice(0, leftDot);
                const fromPort = left.slice(leftDot + 1);
                const toNode = right.slice(0, rightDot);
                const toPort = right.slice(rightDot + 1);
                const fromRuntime = nodeRuntime.get(fromNode) || LOCAL_RUNTIME_ID;
                const toRuntime = nodeRuntime.get(toNode) || LOCAL_RUNTIME_ID;
                if (!nodesById.get(fromNode) || !nodesById.get(toNode)) return;
                if (fromRuntime !== toRuntime) {
                        crossRuntimeEdges.push({ id: `cross-${index}`, fromRuntime, toRuntime, fromNode, fromPort, toNode, toPort });
                        return;
                }
                const lane = runtimeMap.get(fromRuntime);
                lane.edges.push({ id: `edge-${index}`, fromNode, fromPort, toNode, toPort });
        });

        runtimeMap.forEach((runtime) => {
                runtime.edges = runtime.edges.map((edge, index) => {
                        if (typeof edge !== 'string') {
                                return edge;
                        }
                        const parsed = splitEdgeText(edge);
                        return parsed ? { id: `draft-edge-${index}`, ...parsed } : edge;
                }).filter((edge) => edge && typeof edge !== 'string');
        });

        const allEdges = [...crossRuntimeEdges, ...Array.from(runtimeMap.values()).flatMap((runtime) => runtime.edges)];
        runtimeMap.forEach((runtime) => runtime.nodes.forEach((node) => {
                node.connectedInputs = [...new Set(allEdges.filter((edge) => edge.toNode === node.id).map((edge) => edge.toPort || 'image'))];
                node.connectedOutputs = [...new Set(allEdges.filter((edge) => edge.fromNode === node.id).map((edge) => edge.fromPort || 'image'))];
                const visibility = nodePortVisibility?.[node.id];
                const inputNames = new Set(node.inputs.map((port) => port.name));
                const outputNames = new Set(node.outputs.map((port) => port.name));
                node.visibleInputs = visibility && Array.isArray(visibility.inputs)
                        ? visibility.inputs.filter((name) => inputNames.has(name))
                        : [node.inputs[0]?.name].filter(Boolean);
                node.visibleOutputs = visibility && Array.isArray(visibility.outputs)
                        ? visibility.outputs.filter((name) => outputNames.has(name))
                        : [node.outputs[0]?.name].filter(Boolean);
                node.height = NODE_HEADER_HEIGHT + Math.max(1, node.visibleInputs.length, node.visibleOutputs.length) * NODE_PORT_ROW_HEIGHT;
        }));

        const runtimes = Array.from(runtimeMap.values()).filter((runtime) => runtime.nodes.length > 0 || runtime.id === LOCAL_RUNTIME_ID || draftRuntimeIds.has(runtime.id)).map((runtime, index) => {
                const defaultRect = runtimeDefaultRect(index);
                const layoutRect = runtimeLayouts?.[runtime.id];
                const isLocalRuntime = runtime.id === LOCAL_RUNTIME_ID;
                const runtimeState = isLocalRuntime ? (runtimeStatus || 'down') : (remoteRuntimeStatuses?.[runtime.id] || 'unknown');
                return {
                        ...runtime,
                        displayName: runtimeNames?.[runtime.id] || '',
                        status: runtimeState,
                        rect: {
                                x: layoutRect?.x ?? defaultRect.x,
                                y: layoutRect?.y ?? defaultRect.y,
                                w: layoutRect?.w ?? defaultRect.w,
                                h: layoutRect?.h ?? defaultRect.h
                        }
                };
        });
        const nodeIds = runtimes.flatMap((runtime) => runtime.nodes.map((node) => node.id));
        return { runtimes, crossRuntimeEdges, nodeIds };
}

function splitEdgeText(edgeText) {
        const arrowPos = edgeText.indexOf('->');
        if (arrowPos < 0) {
                return null;
        }
        const left = edgeText.slice(0, arrowPos).trim();
        const right = edgeText.slice(arrowPos + 2).trim();
        if (!left || !right) {
                return null;
        }
        const leftDot = left.lastIndexOf('.');
        const rightDot = right.lastIndexOf('.');
        return {
                fromNode: leftDot > 0 ? left.slice(0, leftDot) : left,
                fromPort: leftDot > 0 ? left.slice(leftDot + 1) : '',
                toNode: rightDot > 0 ? right.slice(0, rightDot) : right,
                toPort: rightDot > 0 ? right.slice(rightDot + 1) : ''
        };
}

function edgeToText(edge) {
        if (!edge) return '';
        if (typeof edge === 'string') {
                return edge;
        }
        const fromNode = String(edge.fromNode || edge.from || '');
        const toNode = String(edge.toNode || edge.to || '');
        const fromPort = String(edge.fromPort || '').trim();
        const toPort = String(edge.toPort || '').trim();
        const fromText = fromPort ? `${fromNode}.${fromPort}` : fromNode;
        const toText = toPort ? `${toNode}.${toPort}` : toNode;
        return fromText && toText ? `${fromText} -> ${toText}` : '';
}

function renameEdgeNode(edge, previousNodeId, nextNodeId) {
        const parsed = splitEdgeText(edgeToText(edge));
        if (!parsed) {
                return edgeToText(edge);
        }
        return edgeToText({
                ...parsed,
                fromNode: parsed.fromNode === previousNodeId ? nextNodeId : parsed.fromNode,
                toNode: parsed.toNode === previousNodeId ? nextNodeId : parsed.toNode
        });
}

function normalizeEdgeKey(edgeText) {
        const parsed = splitEdgeText(edgeText);
        if (!parsed || !parsed.fromNode || !parsed.toNode) {
                return '';
        }
        const fromPort = parsed.fromPort && parsed.fromPort !== 'output' ? parsed.fromPort : 'image';
        const toPort = parsed.toPort && parsed.toPort !== 'input' ? parsed.toPort : 'image';
        return `${parsed.fromNode}.${fromPort}->${parsed.toNode}.${toPort}`;
}

function edgeExistsInGraph(edgeText, pipelineGraph, localDraftRuntimes) {
        const targetKey = normalizeEdgeKey(edgeText);
        if (!targetKey) {
                return false;
        }
        const payload = buildGraphPayload(pipelineGraph, localDraftRuntimes);
        return (payload.edges || []).some((entry) => normalizeEdgeKey(entry) === targetKey);
}

function buildGraphPayload(pipelineGraph, localDraftRuntimes, extraEdges = []) {
        const nodes = [];
        const nodeIds = new Set();
        const addNode = (node) => {
                if (!node || !node.id || nodeIds.has(node.id)) {
                        return;
                }
                nodeIds.add(node.id);
                nodes.push({
                        id: node.id,
                        type: node.type || 'node',
                        parameters: { ...(node.parameters || {}) }
                });
        };

        (Array.isArray(pipelineGraph?.nodes) ? pipelineGraph.nodes : []).forEach(addNode);
        (Array.isArray(localDraftRuntimes) ? localDraftRuntimes : []).forEach((runtime) => {
                (runtime.nodes || []).forEach(addNode);
        });

        const edges = [];
        const edgeKeys = new Set();
        const addEdge = (edge) => {
                const text = edgeToText(edge);
                if (!text || edgeKeys.has(text)) {
                        return;
                }
                edgeKeys.add(text);
                edges.push(text);
        };

        (Array.isArray(pipelineGraph?.edges) ? pipelineGraph.edges : []).forEach(addEdge);
        (Array.isArray(localDraftRuntimes) ? localDraftRuntimes : []).forEach((runtime) => {
                (runtime.edges || []).forEach(addEdge);
        });
        (Array.isArray(extraEdges) ? extraEdges : []).forEach(addEdge);

        return { nodes, edges };
}

function edgeCurvePath(fromNode, toNode, edge) {
        return routedEdgeCurvePath(fromNode, toNode, edge);
}

function readInitialViewMode() {
        try {
                const storedMode = window.localStorage.getItem(VIEW_MODE_STORAGE_KEY);
                if (storedMode === 'editor' || storedMode === 'viewer') {
                        return storedMode;
                }
        } catch (_) {
                // Ignore storage access errors and fall back to default mode.
        }
        return DEFAULT_MODE;
}

function readInitialSelectedNodeId() {
        try {
                return String(window.localStorage.getItem(SELECTED_NODE_STORAGE_KEY) || '').trim();
        } catch (_) {
                return '';
        }
}

function normalizeFrameViewerSettings(settings) {
        return {
                debayerEnabled: settings?.debayerEnabled === true
        };
}

function readFrameViewerSettings() {
        try {
                const raw = window.localStorage.getItem(FRAME_VIEWER_SETTINGS_STORAGE_KEY);
                const parsed = raw ? JSON.parse(raw) : {};
                if (!parsed || typeof parsed !== 'object' || Array.isArray(parsed)) {
                        return {};
                }

                const result = {};
                Object.entries(parsed).forEach(([nodeId, settings]) => {
                        const normalizedNodeId = String(nodeId || '').trim();
                        if (normalizedNodeId) {
                                result[normalizedNodeId] = normalizeFrameViewerSettings(settings);
                        }
                });
                return result;
        } catch (_) {
                return {};
        }
}

function readInitialRuntimeLayouts() {
        try {
                const raw = window.localStorage.getItem(RUNTIME_LAYOUTS_STORAGE_KEY);
                if (!raw) {
                        return {};
                }
                const parsed = JSON.parse(raw);
                if (!parsed || typeof parsed !== 'object') {
                        return {};
                }
                const result = {};
                Object.entries(parsed).forEach(([runtimeId, rect]) => {
                        const x = Number(rect?.x);
                        const y = Number(rect?.y);
                        const w = Number(rect?.w);
                        const h = Number(rect?.h);
                        if (!Number.isFinite(x) || !Number.isFinite(y) || !Number.isFinite(w) || !Number.isFinite(h)) {
                                return;
                        }
                        if (w <= 0 || h <= 0) {
                                return;
                        }
                        result[runtimeId] = { x, y, w, h };
                });
                return result;
        } catch (_) {
                return {};
        }
}

function readInitialNodeLayouts() {
        try {
                const raw = window.localStorage.getItem(NODE_LAYOUTS_STORAGE_KEY);
                if (!raw) {
                        return {};
                }
                const parsed = JSON.parse(raw);
                if (!parsed || typeof parsed !== 'object') {
                        return {};
                }
                const result = {};
                Object.entries(parsed).forEach(([nodeId, pos]) => {
                        const x = Number(pos?.x);
                        const y = Number(pos?.y);
                        if (!Number.isFinite(x) || !Number.isFinite(y)) {
                                return;
                        }
                        result[nodeId] = { x, y };
                });
                return result;
        } catch (_) {
                return {};
        }
}

function readInitialNodeNames() {
        try {
                const parsed = JSON.parse(window.localStorage.getItem(NODE_NAMES_STORAGE_KEY) || '{}');
                return parsed && typeof parsed === 'object' ? parsed : {};
        } catch (_) {
                return {};
        }
}

function readInitialRuntimeNames() {
        try {
                const parsed = JSON.parse(window.localStorage.getItem(RUNTIME_NAMES_STORAGE_KEY) || '{}');
                return parsed && typeof parsed === 'object' ? parsed : {};
        } catch (_) {
                return {};
        }
}

function readInitialNodePortVisibility() {
        try {
                const parsed = JSON.parse(window.localStorage.getItem(NODE_PORT_VISIBILITY_STORAGE_KEY) || '{}');
                return parsed && typeof parsed === 'object' ? parsed : {};
        } catch (_) {
                return {};
        }
}

export default function App() {
        const sourceNodeId = (window.CAMFLOW_SOURCE_NODE_ID || 'v4l2src0').trim() || 'v4l2src0';
        const localIp = window.location.hostname || '127.0.0.1';

        const [status, setStatus] = useState('starting');
        const [versionText, setVersionText] = useState('loading version');
        const [graphStatusText, setGraphStatusText] = useState('discovering runtime graph');
        const [pipelineGraph, setPipelineGraph] = useState({ nodes: [], edges: [] });
        const [nodeCatalog, setNodeCatalog] = useState(FALLBACK_NODE_TYPES);
        const [localDraftRuntimes, setLocalDraftRuntimes] = useState([]);
        const [selectedNodeId, setSelectedNodeId] = useState(readInitialSelectedNodeId);
        const [selectedRuntimeId, setSelectedRuntimeId] = useState(LOCAL_RUNTIME_ID);
        const [selectedRuntimeName, setSelectedRuntimeName] = useState('runtime local');
        const [frameContextState, setFrameContextState] = useState(null);
        const [selectedImageKey, setSelectedImageKey] = useState('image');
        const [frameMeta, setFrameMeta] = useState(null);
        const [selectedNodeParams, setSelectedNodeParams] = useState([]);
        const [selectedNodeMeta, setSelectedNodeMeta] = useState(null);
        const [selectedMediaElement, setSelectedMediaElement] = useState(null);
        const [debayerEnabled, setDebayerEnabled] = useState(false);
        const [viewMode, setViewMode] = useState(readInitialViewMode);
        const [shortcutPanelOpen, setShortcutPanelOpen] = useState(false);
        const [parameterFilterOpenRequest, setParameterFilterOpenRequest] = useState(0);
        const [filterCloseRequest, setFilterCloseRequest] = useState(0);
        const [runtimeLogFilterOpenRequest, setRuntimeLogFilterOpenRequest] = useState({ runtimeId: '', sequence: 0 });
        const [runtimeMenu, setRuntimeMenu] = useState({ open: false, x: 0, y: 0, kind: 'background', runtimeId: null, nodeId: null, portDirection: '', portName: '', sources: [], processors: [], sinks: [] });
        const [dialogState, setDialogState] = useState({ open: false, mode: null, runtimeId: null, runtimeName: '', runtimeIp: '', nodeType: '', nodeId: '' });
        const [editorError, setEditorError] = useState('');
        const [runtimeLogs, setRuntimeLogs] = useState({});
        const [runtimeLogPanels, setRuntimeLogPanels] = useState({});
        const [runtimeLayouts, setRuntimeLayouts] = useState(readInitialRuntimeLayouts);
        const [runtimeViewports, setRuntimeViewports] = useState({});
        const [nodeLayouts, setNodeLayouts] = useState(readInitialNodeLayouts);
        const [nodeNames, setNodeNames] = useState(readInitialNodeNames);
        const [runtimeNames, setRuntimeNames] = useState(readInitialRuntimeNames);
        const [nodePortVisibility, setNodePortVisibility] = useState(readInitialNodePortVisibility);
        const [remoteRuntimeStatuses, setRemoteRuntimeStatuses] = useState({});
        const [dragState, setDragState] = useState(null);
        const [editorZoom, setEditorZoom] = useState(1);
        const [editorPanX, setEditorPanX] = useState(0);
        const [editorPanY, setEditorPanY] = useState(0);
        const [editorPanning, setEditorPanning] = useState(false);
        const [editorPanOrigin, setEditorPanOrigin] = useState({ x: 0, y: 0, baseX: 0, baseY: 0 });
        const [viewZoom, setViewZoom] = useState(1);
        const [viewPanX, setViewPanX] = useState(0);
        const [viewPanY, setViewPanY] = useState(0);
        const [frameViewportSize, setFrameViewportSize] = useState({ width: 0, height: 0 });
        const [isPanning, setIsPanning] = useState(false);
        const [panOrigin, setPanOrigin] = useState({ x: 0, y: 0, baseX: 0, baseY: 0 });
        const [splitRatio, setSplitRatio] = useState(0.68);
        const [splitDragState, setSplitDragState] = useState(null);
        const [captureFps, setCaptureFps] = useState(0);
        const [renderFps, setRenderFps] = useState(0);

        const frameCanvasRef = useRef(null);
        const frameViewportRef = useRef(null);
        const editorViewportRef = useRef(null);
        const mainLayoutRef = useRef(null);
        const frameSocketRef = useRef(null);
        const runtimeLogSocketRef = useRef(new Map());
        const runtimeLogSeenRef = useRef(new Map());
        const runtimeLogUiSeqRef = useRef(0);
        const editorPanGestureRef = useRef({ active: false, moved: false, startX: 0, startY: 0, button: null });
        const viewerPanGestureRef = useRef({ active: false, moved: false, startX: 0, startY: 0, button: null });
        const renderCacheRef = useRef({ key: '', rgba: null, width: 0, height: 0 });
        const latestFrameBinaryRef = useRef(null);
        const lastFramePacketRef = useRef(null);
        const frameBinaryProcessingRef = useRef(false);
        const pendingParameterUpdateTimersRef = useRef(new Map());
        const lastRenderedSequenceRef = useRef(-1);
        const expectedFrameSequenceRef = useRef(-1);
        const committedParameterValuesRef = useRef(new Map());
        const lastCaptureRef = useRef({ seq: -1, ts: 0 });
        const lastRenderWallMsRef = useRef(0);
        const lastFrameSeenAtRef = useRef(0);
        const statusRef = useRef('starting');
        const selectedRuntimeIdRef = useRef(selectedRuntimeId);
        const parameterTypeaheadRef = useRef({ text: '', timestamp: 0, focusedElement: null });
        const debayerEnabledRef = useRef(debayerEnabled);
        const suppressNextNodeSelectRef = useRef(false);
        const suppressNextParameterReloadRef = useRef(false);
        const hasAutoCenteredEditorRef = useRef(false);
        const hasInitializedPipelineSelectionRef = useRef(false);
        const frameViewportScaleRef = useRef(0);
        const frameViewerSettingsRef = useRef(readFrameViewerSettings());

        const editorGraph = useMemo(
                () => buildEditorGraph(pipelineGraph, localDraftRuntimes, localIp, runtimeLayouts, nodeLayouts, status, remoteRuntimeStatuses, nodeCatalog, nodeNames, runtimeNames, nodePortVisibility),
                [pipelineGraph, localDraftRuntimes, localIp, runtimeLayouts, nodeLayouts, status, remoteRuntimeStatuses, nodeCatalog, nodeNames, runtimeNames, nodePortVisibility]
        );
        const editorNodeIdsSignature = editorGraph.nodeIds.join('|');
        const displayedFrameNodeId = String(frameContextState?.nodeId || selectedNodeId || sourceNodeId).trim();
        const displayedFrameNodeName = useMemo(() => {
                const liveNode = (pipelineGraph.nodes || []).find((node) => node.id === displayedFrameNodeId);
                if (liveNode) {
                        return String(liveNode.name || liveNode.id || displayedFrameNodeId);
                }

                const draftNode = localDraftRuntimes.flatMap((runtime) => runtime.nodes || []).find((node) => node.id === displayedFrameNodeId);
                return String(draftNode?.name || draftNode?.id || displayedFrameNodeId);
        }, [displayedFrameNodeId, pipelineGraph.nodes, localDraftRuntimes]);

        const { toggleRemoteRuntime } = useRemoteRuntimeStatus({
                runtimes: editorGraph.runtimes,
                localRuntimeId: LOCAL_RUNTIME_ID,
                runtimeBaseUrl,
                setRemoteRuntimeStatuses
        });
        const liveNodeIds = useMemo(() => new Set((pipelineGraph.nodes || []).map((node) => node.id)), [pipelineGraph.nodes]);
        const runtimeLogTargets = useMemo(() => {
                const targets = [];
                for (const runtime of editorGraph.runtimes) {
                        if (runtime.status === 'down') {
                                continue;
                        }
                        if (runtimeLogPanels[runtime.id] === false) {
                                continue;
                        }
                        const socketUrl = runtimeLogWsUrl(runtime, localIp);
                        if (!socketUrl) {
                                continue;
                        }
                        targets.push({ runtimeId: runtime.id, socketUrl });
                }
                targets.sort((a, b) => a.runtimeId.localeCompare(b.runtimeId));
                return targets;
        }, [editorGraph.runtimes, localIp, runtimeLogPanels]);
        const runtimeLogTargetsSignature = useMemo(
                () => runtimeLogTargets.map((item) => `${item.runtimeId}:${item.socketUrl}`).join('|'),
                [runtimeLogTargets]
        );

        // Keep control refs in sync on every render so websocket callbacks and
        // local re-render paths use exactly the same current control values.
        debayerEnabledRef.current = debayerEnabled;

        useEffect(() => {
                try {
                        window.localStorage.setItem(VIEW_MODE_STORAGE_KEY, viewMode);
                } catch (_) {
                        // Ignore storage access errors.
                }
        }, [viewMode]);

        useEffect(() => {
                try {
                        if (selectedNodeId) {
                                window.localStorage.setItem(SELECTED_NODE_STORAGE_KEY, selectedNodeId);
                        } else {
                                window.localStorage.removeItem(SELECTED_NODE_STORAGE_KEY);
                        }
                } catch (_) {
                        // Ignore storage access errors.
                }
        }, [selectedNodeId]);

        useEffect(() => {
                try {
                        window.localStorage.setItem(RUNTIME_LAYOUTS_STORAGE_KEY, JSON.stringify(runtimeLayouts));
                } catch (_) {
                        // Ignore storage access errors.
                }
        }, [runtimeLayouts]);

        useEffect(() => {
                try {
                        window.localStorage.setItem(NODE_LAYOUTS_STORAGE_KEY, JSON.stringify(nodeLayouts));
                } catch (_) {
                        // Ignore storage access errors.
                }
        }, [nodeLayouts]);

        useEffect(() => {
                try {
                        window.localStorage.setItem(NODE_NAMES_STORAGE_KEY, JSON.stringify(nodeNames));
                } catch (_) {
                        // Ignore storage access errors.
                }
        }, [nodeNames]);

        useEffect(() => {
                try {
                        window.localStorage.setItem(RUNTIME_NAMES_STORAGE_KEY, JSON.stringify(runtimeNames));
                } catch (_) {
                        // Ignore storage access errors.
                }
        }, [runtimeNames]);

        useEffect(() => {
                try {
                        window.localStorage.setItem(NODE_PORT_VISIBILITY_STORAGE_KEY, JSON.stringify(nodePortVisibility));
                } catch (_) {
                        // Ignore storage access errors.
                }
        }, [nodePortVisibility]);

        useEffect(() => {
                const settings = frameViewerSettingsRef.current[displayedFrameNodeId] || DEFAULT_FRAME_VIEWER_SETTINGS;
                setDebayerEnabled(settings.debayerEnabled);
                renderCacheRef.current.key = '';
        }, [displayedFrameNodeId]);

        function storeFrameViewerSettings(nodeId, settings) {
                if (!nodeId) {
                        return;
                }

                frameViewerSettingsRef.current = {
                        ...frameViewerSettingsRef.current,
                        [nodeId]: normalizeFrameViewerSettings(settings)
                };
                try {
                        window.localStorage.setItem(FRAME_VIEWER_SETTINGS_STORAGE_KEY, JSON.stringify(frameViewerSettingsRef.current));
                } catch (_) {
                        // Ignore storage access errors.
                }
        }

        function updateDebayerEnabled(nextValueOrUpdater) {
                setDebayerEnabled((currentValue) => {
                        const nextValue = typeof nextValueOrUpdater === 'function' ? nextValueOrUpdater(currentValue) : nextValueOrUpdater;
                        const normalizedSettings = normalizeFrameViewerSettings({ debayerEnabled: nextValue });
                        storeFrameViewerSettings(displayedFrameNodeId, normalizedSettings);
                        return normalizedSettings.debayerEnabled;
                });
        }

        function updateFrameViewportSize() {
                const viewport = frameViewportRef.current;
                const canvas = frameCanvasRef.current;
                const width = viewport?.clientWidth || 0;
                const height = viewport?.clientHeight || 0;
                if (width <= 0 || height <= 0) {
                        return;
                }

                const nextBaseScale = canvas?.width && canvas?.height
                        ? Math.min(width / canvas.width, height / canvas.height)
                        : 0;
                const previousBaseScale = frameViewportScaleRef.current;
                if (previousBaseScale > 0 && nextBaseScale > 0 && previousBaseScale !== nextBaseScale) {
                        const scaleRatio = nextBaseScale / previousBaseScale;
                        setViewPanX((current) => current * scaleRatio);
                        setViewPanY((current) => current * scaleRatio);
                }
                frameViewportScaleRef.current = nextBaseScale;
                setFrameViewportSize((current) => current.width === width && current.height === height ? current : { width, height });
        }

        function cleanupDrafts(drafts) {
                return (drafts || []).map((runtime) => ({
                        ...runtime,
                        nodes: (runtime.nodes || []).filter((node) => !isAutoNodeId(node.id)),
                        edges: (runtime.edges || []).filter((edge) => {
                                const parsed = splitEdgeText(edgeToText(edge));
                                return !(parsed && (isAutoNodeId(parsed.fromNode) || isAutoNodeId(parsed.toNode)));
                        })
                }));
        }

        function runtimeRectById(runtimeId) {
                const index = editorGraph.runtimes.findIndex((runtime) => runtime.id === runtimeId);
                if (index < 0) {
                        return runtimeDefaultRect(0);
                }
                const runtime = editorGraph.runtimes[index];
                return runtime.rect || runtimeDefaultRect(index);
        }

        function canPlaceRuntime(runtimeId, nextRect) {
                if (nextRect.w < RUNTIME_MIN_WIDTH || nextRect.h < RUNTIME_MIN_HEIGHT) {
                        return false;
                }
                return editorGraph.runtimes.every((runtime) => {
                        if (runtime.id === runtimeId) {
                                return true;
                        }
                        return !rectsOverlap(nextRect, runtime.rect || runtimeDefaultRect(0));
                });
        }

        function closeMenu() {
                setRuntimeMenu((menu) => ({ ...menu, open: false }));
        }

        function openMenu(event, kind, runtimeId = null, nodeId = null, portDirection = '', portName = '') {
                if (event.button === 2 && editorPanGestureRef.current.moved) {
                        event.preventDefault();
                        event.stopPropagation();
                        editorPanGestureRef.current = { active: false, moved: false, startX: 0, startY: 0, button: null };
                        return;
                }
                event.preventDefault();
                event.stopPropagation();
                editorPanGestureRef.current = { active: false, moved: false, startX: 0, startY: 0, button: null };
                setEditorPanning(false);
                setRuntimeMenu({
                        open: true,
                        x: event.clientX,
                        y: event.clientY,
                        kind,
                        runtimeId,
                        nodeId,
                        portDirection,
                        portName,
                        sources: nodeCatalog.sources.length ? nodeCatalog.sources : FALLBACK_NODE_TYPES.sources,
                        processors: nodeCatalog.processors.length ? nodeCatalog.processors : FALLBACK_NODE_TYPES.processors,
                        sinks: nodeCatalog.sinks.length ? nodeCatalog.sinks : FALLBACK_NODE_TYPES.sinks
                });
        }

        function openRuntimeDialog() {
                const hostDefault = localIp && localIp !== '127.0.0.1' ? localIp : 'localhost';
                setDialogState({ open: true, mode: 'runtime', runtimeId: null, runtimeName: nextName('runtime', editorGraph.runtimes), runtimeIp: hostDefault, nodeType: '', nodeId: '' });
        }

        async function persistGraph(nextDraftRuntimes, extraEdges = []) {
                try {
                        const runtimeState = await getRuntimeStatus();
                        if (runtimeState.state !== 'stopped') {
                                await setRuntimeStopped(true);
                                setStatus('stopped');
                        }

                        const payload = buildGraphPayload(pipelineGraph, nextDraftRuntimes, extraEdges);
                        await savePipeline(payload);
                        await fetchPipeline();
                        setLocalDraftRuntimes((current) => cleanupDrafts(current));
                        return true;
                } catch (_) {
                        setEditorError('failed to save graph');
                        return false;
                }
        }

        async function ensurePipelineStopped() {
                try {
                        const runtimeState = await getRuntimeStatus();
                        if (runtimeState.state !== 'stopped') {
                                await setRuntimeStopped(true);
                                setStatus('stopped');
                        }
                        return true;
                } catch (_) {
                        return false;
                }
        }

        async function openNodeDialog(runtimeId, nodeType) {
                const runtime = editorGraph.runtimes.find((item) => item.id === runtimeId) || editorGraph.runtimes[0];
                const nodeId = autoNodeId();
                const runtimeIp = runtime?.ip || localIp;
                const runtimeNodeCount = (editorGraph.runtimes.find((item) => item.id === runtimeId)?.nodes || []).length;
                const defaultNodePos = { x: 80 + runtimeNodeCount * 180, y: 64 + Math.floor(runtimeNodeCount / 4) * 92 };
                let nextNodePos = defaultNodePos;

                if (runtime && editorViewportRef.current) {
                        const rect = editorViewportRef.current.getBoundingClientRect();
                        const graphX = (runtimeMenu.x - rect.left - editorPanX) / Math.max(editorZoom, 0.0001);
                        const graphY = (runtimeMenu.y - rect.top - editorPanY) / Math.max(editorZoom, 0.0001);
                        const runtimeRect = runtimeRectById(runtime.id);
                        const runtimeViewport = runtimeViewports[runtime.id] || DEFAULT_RUNTIME_VIEWPORT;
                        nextNodePos = {
                                x: (graphX - runtimeRect.x - runtimeViewport.panX) / runtimeViewport.zoom - NODE_WIDTH * 0.5,
                                y: (graphY - runtimeRect.y - RUNTIME_HEADER_HEIGHT - runtimeViewport.panY) / runtimeViewport.zoom - NODE_HEIGHT * 0.5
                        };
                }
                setLocalDraftRuntimes((current) => {
                        if (current.some((entry) => entry.id === runtimeId)) {
                                return current;
                        }
                        return [
                                ...current,
                                {
                                        id: runtimeId,
                                        name: runtime?.name || runtimeId,
                                        ip: runtimeIp,
                                        nodes: [],
                                        edges: []
                                }
                        ];
                });
                setSelectedRuntimeId(runtimeId);
                setSelectedRuntimeName(runtime?.name || runtimeId);
                setDialogState({ open: false, mode: null, runtimeId: null, runtimeName: '', runtimeIp: '', nodeType: '', nodeId: '' });

                try {
                        if (!await ensurePipelineStopped()) {
                                setEditorError('failed to stop runtime');
                                return;
                        }

                        const payload = await addNode(
                                runtimeId === LOCAL_RUNTIME_ID || runtimeIp === localIp || runtimeIp === '127.0.0.1' || runtimeIp === 'localhost'
                                        ? {
                                                id: nodeId,
                                                type: nodeType
                                        }
                                        : {
                                                id: nodeId,
                                                type: nodeType,
                                                runtimeTargetIp: runtimeIp
                                        }
                        );
                        const actualNodeId = payload.id || nodeId;
                        setPipelineGraph((current) => {
                                const nodes = Array.isArray(current?.nodes) ? current.nodes : [];
                                if (nodes.some((node) => node.id === actualNodeId)) {
                                        return current;
                                }
                                const nextNode = {
                                        id: actualNodeId,
                                        type: nodeType || 'node',
                                        parameters: {
                                                ...(runtimeId === LOCAL_RUNTIME_ID || runtimeIp === localIp || runtimeIp === '127.0.0.1' || runtimeIp === 'localhost'
                                                        ? {}
                                                        : { runtimeTargetIp: runtimeIp })
                                        }
                                };
                                return {
                                        ...current,
                                        nodes: [...nodes, nextNode]
                                };
                        });
                        setNodeLayouts((current) => ({ ...current, [actualNodeId]: nextNodePos }));
                        setLocalDraftRuntimes((current) => cleanupDrafts(current));
                } catch (_) {
                        setEditorError('failed to add node');
                }
        }

        function addRuntime() {
                const runtimeId = `runtime-${Date.now()}`;
                const runtimeName = (dialogState.runtimeName || nextName('runtime', editorGraph.runtimes)).trim();
                const runtimeIp = (dialogState.runtimeIp || (localIp && localIp !== '127.0.0.1' ? localIp : 'localhost')).trim();
                if (!runtimeName || !runtimeIp) {
                        setEditorError('runtime name and ip are required');
                        return;
                }
                const index = editorGraph.runtimes.length;
                const defaultRect = runtimeDefaultRect(index);
                setRuntimeLayouts((current) => ({ ...current, [runtimeId]: defaultRect }));
                setLocalDraftRuntimes((current) => [...current, { id: runtimeId, name: runtimeName, ip: runtimeIp, nodes: [], edges: [] }]);
                setSelectedRuntimeId(runtimeId);
                setSelectedRuntimeName(runtimeName);
                setDialogState({ open: false, mode: null, runtimeId: null, runtimeName: '', runtimeIp: '', nodeType: '', nodeId: '' });
        }

        function startRuntimeDrag(event, runtimeId) {
                if (event.button !== 0) {
                        return;
                }
                if (isInteractiveHeaderTarget(event.target)) {
                        return;
                }
                const rect = runtimeRectById(runtimeId);
                setDragState({ kind: 'runtime', runtimeId, startX: event.clientX, startY: event.clientY, startRect: rect });
        }

        function startRuntimeResize(event, runtimeId) {
                if (event.button !== 0) {
                        return;
                }
                event.preventDefault();
                event.stopPropagation();
                const rect = runtimeRectById(runtimeId);
                setDragState({ kind: 'runtime-resize', runtimeId, startX: event.clientX, startY: event.clientY, startRect: rect });
        }

        function startNodeDrag(event, runtimeId, nodeId) {
                if (event.button !== 0) {
                        return;
                }
                if (event.target instanceof Element && event.target.closest('.node-port-side')) {
                        return;
                }
                event.stopPropagation();
                const runtime = editorGraph.runtimes.find((item) => item.id === runtimeId);
                const node = runtime?.nodes?.find((item) => item.id === nodeId);
                if (!node) {
                        return;
                }
                const runtimeZoom = runtimeViewports[runtimeId]?.zoom || 1;
                setDragState({ kind: 'node', runtimeId, nodeId, startX: event.clientX, startY: event.clientY, runtimeZoom, startPos: { x: node.x, y: node.y } });
        }

        function updateRuntimeViewport(runtimeId, update) {
                setRuntimeViewports((current) => {
                        const currentViewport = current[runtimeId] || DEFAULT_RUNTIME_VIEWPORT;
                        const nextViewport = typeof update === 'function' ? update(currentViewport) : update;
                        return { ...current, [runtimeId]: nextViewport };
                });
        }

        function fitEditorView() {
                const viewport = editorViewportRef.current;
                if (!viewport || editorGraph.runtimes.length === 0) {
                        setEditorZoom(1);
                        setEditorPanX(0);
                        setEditorPanY(0);
                        return false;
                }
                if (viewport.clientWidth === 0 || viewport.clientHeight === 0) {
                        return false;
                }

                const bounds = editorGraph.runtimes.reduce((current, runtime) => {
                        const rect = runtime.rect || { x: 0, y: 0, w: 0, h: 0 };
                        return {
                                minX: Math.min(current.minX, rect.x),
                                minY: Math.min(current.minY, rect.y),
                                maxX: Math.max(current.maxX, rect.x + rect.w),
                                maxY: Math.max(current.maxY, rect.y + rect.h)
                        };
                }, { minX: Infinity, minY: Infinity, maxX: -Infinity, maxY: -Infinity });
                const contentWidth = Math.max(1, bounds.maxX - bounds.minX);
                const contentHeight = Math.max(1, bounds.maxY - bounds.minY);
                const padding = 24;
                const availableWidth = Math.max(1, viewport.clientWidth - padding * 2);
                const availableHeight = Math.max(1, viewport.clientHeight - padding * 2);
                const zoom = Math.max(0.01, Math.min(1, availableWidth / contentWidth, availableHeight / contentHeight));
                setEditorZoom(zoom);
                setEditorPanX((viewport.clientWidth - contentWidth * zoom) / 2 - bounds.minX * zoom);
                setEditorPanY((viewport.clientHeight - contentHeight * zoom) / 2 - bounds.minY * zoom);
                return true;
        }

        async function connectNodes(sourceNodeId, targetNodeId, sourcePort = 'image', targetPort = 'image') {
                if (!sourceNodeId || !targetNodeId || sourceNodeId === targetNodeId) {
                        return;
                }
                const edgeText = `${sourceNodeId}.${sourcePort} -> ${targetNodeId}.${targetPort}`;

                if (edgeExistsInGraph(edgeText, pipelineGraph, localDraftRuntimes)) {
                        setEditorError('edge already exists');
                        return;
                }

                try {
                        if (!await ensurePipelineStopped()) {
                                setEditorError('failed to stop runtime');
                                return;
                        }

                        await createEdge(edgeText);
                        setPipelineGraph((current) => {
                                const edges = Array.isArray(current?.edges) ? current.edges : [];
                                const exists = edges.some((entry) => normalizeEdgeKey(entry) === normalizeEdgeKey(edgeText));
                                if (exists) {
                                        return current;
                                }
                                return {
                                        ...current,
                                        edges: [...edges, edgeText]
                                };
                        });
                } catch (_) {
                        setEditorError('failed to create edge');
                }
        }

        async function deleteEdgeByText(edgeText) {
                if (!edgeText) {
                        return;
                }

                try {
                        if (!await ensurePipelineStopped()) {
                                setEditorError('failed to stop runtime');
                                return;
                        }

                        await deleteEdgeApi(edgeText);
                        setPipelineGraph((current) => {
                                const edges = Array.isArray(current?.edges) ? current.edges : [];
                                const targetKey = normalizeEdgeKey(edgeText);
                                const filtered = edges.filter((entry) => normalizeEdgeKey(entry) !== targetKey);
                                if (filtered.length === edges.length) {
                                        return current;
                                }
                                return {
                                        ...current,
                                        edges: filtered
                                };
                        });
                        setEditorError('');
                } catch (_) {
                        setEditorError('failed to delete edge');
                }
        }

        function renameRuntime(runtimeId, nextName) {
                const currentRuntime = editorGraph.runtimes.find((runtime) => runtime.id === runtimeId);
                if (!currentRuntime || !nextName) return;
                setRuntimeNames((current) => ({ ...current, [runtimeId]: nextName }));
                setLocalDraftRuntimes((current) => current.map((runtime) => (runtime.id === runtimeId ? { ...runtime, name: nextName } : runtime)));
                if (runtimeId === selectedRuntimeId) setSelectedRuntimeName(nextName);
        }

        async function renameNode(nodeId, nextName) {
                const nextNodeId = String(nextName || '').trim();
                if (!nextNodeId || nextNodeId === nodeId) return;
                if (editorGraph.nodeIds.includes(nextNodeId)) {
                        setEditorError('node id already exists');
                        return;
                }

                const migrateKey = (current) => {
                        if (!Object.prototype.hasOwnProperty.call(current, nodeId)) return current;
                        const next = { ...current, [nextNodeId]: current[nodeId] };
                        delete next[nodeId];
                        return next;
                };
                const migrateUiState = () => {
                        setNodeNames((current) => {
                                const next = { ...current };
                                delete next[nodeId];
                                return next;
                        });
                        setNodeLayouts(migrateKey);
                        setNodePortVisibility(migrateKey);
                        setSelectedNodeId((current) => {
                                if (current !== nodeId) return current;
                                suppressNextParameterReloadRef.current = true;
                                return nextNodeId;
                        });
                        setSelectedNodeMeta((current) => current?.id === nodeId ? { ...current, id: nextNodeId, name: nextNodeId } : current);
                        setFrameContextState((current) => current?.nodeId === nodeId ? { ...current, nodeId: nextNodeId } : current);
                        for (const key of [...committedParameterValuesRef.current.keys()]) {
                                if (!key.startsWith(`${nodeId}:`)) continue;
                                const value = committedParameterValuesRef.current.get(key);
                                committedParameterValuesRef.current.delete(key);
                                committedParameterValuesRef.current.set(`${nextNodeId}:${key.slice(nodeId.length + 1)}`, value);
                        }
                        if (Object.prototype.hasOwnProperty.call(frameViewerSettingsRef.current, nodeId)) {
                                const nextSettings = { ...frameViewerSettingsRef.current, [nextNodeId]: frameViewerSettingsRef.current[nodeId] };
                                delete nextSettings[nodeId];
                                frameViewerSettingsRef.current = nextSettings;
                                try {
                                        window.localStorage.setItem(FRAME_VIEWER_SETTINGS_STORAGE_KEY, JSON.stringify(nextSettings));
                                } catch (_) {
                                        // Ignore storage write failures.
                                }
                        }
                };

                if (!liveNodeIds.has(nodeId)) {
                        setLocalDraftRuntimes((current) => current.map((runtime) => ({
                                ...runtime,
                                nodes: runtime.nodes.map((node) => node.id === nodeId ? { ...node, id: nextNodeId, name: nextNodeId } : node),
                                edges: runtime.edges.map((edge) => renameEdgeNode(edge, nodeId, nextNodeId))
                        })));
                        migrateUiState();
                        setEditorError('');
                        return;
                }

                try {
                        if (!await ensurePipelineStopped()) {
                                setEditorError('failed to stop runtime');
                                return;
                        }
                        await renameNodeApi(nodeId, nextNodeId);
                        setPipelineGraph((current) => ({
                                ...current,
                                nodes: (current.nodes || []).map((node) => node.id === nodeId ? { ...node, id: nextNodeId, name: nextNodeId } : node),
                                edges: (current.edges || []).map((edge) => renameEdgeNode(edge, nodeId, nextNodeId))
                        }));
                        setLocalDraftRuntimes((current) => current.map((runtime) => ({
                                ...runtime,
                                nodes: runtime.nodes.map((node) => node.id === nodeId ? { ...node, id: nextNodeId, name: nextNodeId } : node),
                                edges: runtime.edges.map((edge) => renameEdgeNode(edge, nodeId, nextNodeId))
                        })));
                        migrateUiState();
                        setEditorError('');
                } catch (_) {
                        setEditorError('failed to rename node');
                }
        }

        async function deleteEdgesForPort(nodeId, direction, portName) {
                const payload = buildGraphPayload(pipelineGraph, []);
                const remainingEdges = payload.edges.filter((edge) => {
                        const parsed = splitEdgeText(edge);
                        if (!parsed) return true;
                        const edgePort = direction === 'input'
                                ? parsed.toPort === 'input' ? 'image' : parsed.toPort
                                : parsed.fromPort === 'output' ? 'image' : parsed.fromPort;
                        return direction === 'input'
                                ? parsed.toNode !== nodeId || edgePort !== portName
                                : parsed.fromNode !== nodeId || edgePort !== portName;
                });
                if (remainingEdges.length === payload.edges.length) {
                        return true;
                }

                try {
                        if (!await ensurePipelineStopped()) {
                                setEditorError('failed to stop runtime');
                                return false;
                        }
                        await savePipeline({ ...payload, edges: remainingEdges });
                        await fetchPipeline();
                        setEditorError('');
                        return true;
                } catch (_) {
                        setEditorError('failed to delete port edges');
                        return false;
                }
        }

        async function hideNodePort(nodeId, direction, portName) {
                if (!nodeId || !direction || !portName || !await deleteEdgesForPort(nodeId, direction, portName)) {
                        return;
                }
                const node = editorGraph.runtimes.flatMap((runtime) => runtime.nodes).find((candidate) => candidate.id === nodeId);
                if (!node) {
                        return;
                }
                const key = direction === 'input' ? 'inputs' : 'outputs';
                const inputs = node.visibleInputs || [];
                const outputs = node.visibleOutputs || [];
                setNodePortVisibility((current) => ({
                        ...current,
                        [nodeId]: {
                                inputs,
                                outputs,
                                [key]: (key === 'inputs' ? inputs : outputs).filter((name) => name !== portName)
                        }
                }));
                closeMenu();
        }

        function edgeTouchesNode(edgeText, nodeId) {
                const edge = splitEdgeText(edgeText);
                return Boolean(edge && (edge.fromNode === nodeId || edge.toNode === nodeId));
        }

        async function deleteNode(nodeId) {
                if (!nodeId) {
                        return;
                }

                const nextDraftRuntimes = localDraftRuntimes.map((runtime) => ({
                        ...runtime,
                        nodes: (runtime.nodes || []).filter((node) => node.id !== nodeId),
                        edges: (runtime.edges || []).filter((edge) => !edgeTouchesNode(edgeToText(edge), nodeId))
                }));

                try {
                        if (!await ensurePipelineStopped()) {
                                setEditorError('failed to stop runtime');
                                return;
                        }

                        await deleteNodeApi(nodeId);

                        setPipelineGraph((current) => {
                                const nodes = Array.isArray(current?.nodes) ? current.nodes : [];
                                const edges = Array.isArray(current?.edges) ? current.edges : [];
                                const nextNodes = nodes.filter((node) => node.id !== nodeId);
                                const nextEdges = edges.filter((edge) => !edgeTouchesNode(edgeToText(edge), nodeId));
                                return {
                                        ...current,
                                        nodes: nextNodes,
                                        edges: nextEdges
                                };
                        });
                        setLocalDraftRuntimes(nextDraftRuntimes);
                        setSelectedNodeId('');
                } catch (_) {
                        setEditorError('failed to delete node');
                }
        }

        async function deleteRuntime(runtimeId) {
                if (!runtimeId || runtimeId === LOCAL_RUNTIME_ID) {
                        return;
                }

                const runtimeNodeIds = new Set(
                        (pipelineGraph.nodes || [])
                                .filter((node) => runtimeIdFromNode(node, localIp) === runtimeId)
                                .map((node) => node.id)
                );

                localDraftRuntimes.forEach((runtime) => {
                        if (runtime.id === runtimeId) {
                                (runtime.nodes || []).forEach((node) => runtimeNodeIds.add(node.id));
                        }
                });

                if (runtimeNodeIds.size === 0) {
                        setLocalDraftRuntimes((current) => current.filter((runtime) => runtime.id !== runtimeId));
                        return;
                }

                const nextDraftRuntimes = localDraftRuntimes
                        .filter((runtime) => runtime.id !== runtimeId)
                        .map((runtime) => ({
                                ...runtime,
                                nodes: (runtime.nodes || []).filter((node) => !runtimeNodeIds.has(node.id)),
                                edges: (runtime.edges || []).filter((edge) => {
                                        const parsed = splitEdgeText(edgeToText(edge));
                                        return !(parsed && (runtimeNodeIds.has(parsed.fromNode) || runtimeNodeIds.has(parsed.toNode)));
                                })
                        }));

                const payload = buildGraphPayload(pipelineGraph, nextDraftRuntimes);
                payload.nodes = (payload.nodes || []).filter((node) => !runtimeNodeIds.has(node.id));
                payload.edges = (payload.edges || []).filter((edge) => {
                        const parsed = splitEdgeText(edge);
                        return !(parsed && (runtimeNodeIds.has(parsed.fromNode) || runtimeNodeIds.has(parsed.toNode)));
                });

                try {
                        const runtimeState = await getRuntimeStatus();
                        if (runtimeState.state !== 'stopped') {
                                await setRuntimeStopped(true);
                                setStatus('stopped');
                        }

                        await savePipeline(payload);

                        setLocalDraftRuntimes(nextDraftRuntimes);
                        if (selectedRuntimeId === runtimeId) {
                                setSelectedRuntimeId(LOCAL_RUNTIME_ID);
                                setSelectedRuntimeName('runtime local');
                        }
                        setSelectedNodeId('');
                        await fetchPipeline();
                } catch (_) {
                        setEditorError('failed to delete runtime');
                }
        }

        function renderRgbaToCanvas(width, height, rgba, resetView = false) {
                const canvas = frameCanvasRef.current;
                if (!canvas) return false;
                if (canvas.width !== width || canvas.height !== height) {
                        canvas.width = width;
                        canvas.height = height;
                        resetView = true;
                }
                const ctx = canvas.getContext('2d');
                if (!ctx) return false;
                ctx.putImageData(new ImageData(rgba, width, height), 0, 0);
                if (resetView) {
                        setViewZoom(1);
                        setViewPanX(0);
                        setViewPanY(0);
                }
                return true;
        }

        function renderRawFrame(meta, bytes, resetView = false) {
                const activeDebayerEnabled = debayerEnabledRef.current;
                const cacheKey = `${meta.sequence}:${meta.formatId}:${meta.bitsPerPixel}:${meta.stride}:${meta.bitShift}:${activeDebayerEnabled ? 1 : 0}`;
                if (renderCacheRef.current.key === cacheKey && renderCacheRef.current.width === meta.width && renderCacheRef.current.height === meta.height && renderCacheRef.current.rgba) {
                        return renderRgbaToCanvas(meta.width, meta.height, renderCacheRef.current.rgba, resetView);
                }

                const rgba = renderPacketToRgba(meta, bytes, activeDebayerEnabled);

                renderCacheRef.current = { key: cacheKey, rgba, width: meta.width, height: meta.height };
                return renderRgbaToCanvas(meta.width, meta.height, rgba, resetView);
        }

        function updateFps(meta) {
                const nowMs = performance.now();
                if (lastRenderWallMsRef.current > 0) {
                        const dtMs = nowMs - lastRenderWallMsRef.current;
                        if (dtMs > 0) {
                                const instantRenderFps = 1000 / dtMs;
                                setRenderFps((current) => (current <= 0 ? instantRenderFps : current * 0.8 + instantRenderFps * 0.2));
                        }
                }
                lastRenderWallMsRef.current = nowMs;

                if (lastCaptureRef.current.seq >= 0 && meta.sequence > lastCaptureRef.current.seq && meta.timestampNs > lastCaptureRef.current.ts) {
                        const ds = meta.sequence - lastCaptureRef.current.seq;
                        const dtNs = meta.timestampNs - lastCaptureRef.current.ts;
                        const instantCaptureFps = (ds * 1_000_000_000) / dtNs;
                        setCaptureFps((current) => (current <= 0 ? instantCaptureFps : current * 0.8 + instantCaptureFps * 0.2));
                }
                lastCaptureRef.current = { seq: meta.sequence, ts: meta.timestampNs };
        }

        async function fetchRuntimeStatus() {
                try {
                        const data = await getRuntimeStatus();
                        setStatus(data.state === 'stopped' ? 'stopped' : 'running');
                } catch (_) {
                        setStatus('down');
                }
        }

        async function fetchRuntimeVersion() {
                try {
                        const version = await getRuntimeVersion();
                        setVersionText(`${version.version} | opencv ${version.opencv} (${version.git}, ${version.build})`);
                } catch (_) {
                        setVersionText('version unavailable');
                }
        }

        async function fetchNodeCatalog() {
                try {
                        setNodeCatalog(await getNodeCatalog());
                } catch (_) {
                        setNodeCatalog(FALLBACK_NODE_TYPES);
                }
        }

        async function fetchPipeline() {
                try {
                        const graph = await getPipeline();
                        setPipelineGraph(graph);
                        const discovery = buildEditorGraph(graph, localDraftRuntimes, localIp, runtimeLayouts, nodeLayouts, status, remoteRuntimeStatuses, nodeCatalog, nodeNames, runtimeNames, nodePortVisibility);
                        setGraphStatusText(`discovered ${discovery.nodeIds.length} node(s) on ${discovery.runtimes.length} runtime(s)`);
                        const storedSelectedNodeId = readInitialSelectedNodeId();
                        const v4l2Sources = (graph.nodes || []).filter((node) => node.type === 'v4l2src');
                        if (!hasInitializedPipelineSelectionRef.current && discovery.nodeIds.length > 0) {
                                const initialNodeId = v4l2Sources.length === 1
                                        ? v4l2Sources[0].id
                                        : (storedSelectedNodeId && discovery.nodeIds.includes(storedSelectedNodeId) ? storedSelectedNodeId : discovery.nodeIds[0]);
                                if (initialNodeId !== selectedNodeId) {
                                        suppressNextParameterReloadRef.current = true;
                                        setSelectedNodeId(initialNodeId);
                                }
                                void fetchNodeParameters(initialNodeId, graph.nodes);
                        } else if (!discovery.nodeIds.includes(selectedNodeId) && discovery.nodeIds.length > 0) {
                                const preferredNodeId = storedSelectedNodeId && discovery.nodeIds.includes(storedSelectedNodeId) ? storedSelectedNodeId : discovery.nodeIds[0];
                                setSelectedNodeId(preferredNodeId);
                        } else if (!discovery.nodeIds.includes(selectedNodeId) && discovery.nodeIds.length === 0) {
                                setSelectedNodeId('');
                        }
                        if (discovery.nodeIds.length > 0) {
                                hasInitializedPipelineSelectionRef.current = true;
                        }
                        if (!discovery.runtimes.some((runtime) => runtime.id === selectedRuntimeId) && discovery.runtimes.length > 0) {
                                setSelectedRuntimeId(discovery.runtimes[0].id);
                                setSelectedRuntimeName(discovery.runtimes[0].name);
                        }
                } catch (_) {
                        setGraphStatusText('runtime discovery failed');
                }
        }

        async function fetchNodeParameters(nodeId, graphNodes = pipelineGraph.nodes) {
                if (!nodeId) return;
                const graphNode = (graphNodes || []).find((node) => node.id === nodeId);
                if (!graphNode) {
                        for (const key of committedParameterValuesRef.current.keys()) {
                                if (key.startsWith(`${nodeId}:`)) {
                                        committedParameterValuesRef.current.delete(key);
                                }
                        }
                        const localNode = localDraftRuntimes.flatMap((runtime) => runtime.nodes || []).find((node) => node.id === nodeId);
                        setSelectedNodeMeta(localNode || null);
                        setSelectedNodeParams(localNode ? [
                                { name: 'type', type: 'string', value: localNode.type, description: 'Node type name' },
                                { name: 'runtimeTargetIp', type: 'string', value: '', description: 'Planned target runtime IP' }
                        ] : []);
                        return;
                }

                try {
                        const payload = await getNodeParameters(nodeId);
                        const parameters = Array.isArray(payload) ? payload : (payload.parameters || []);
                        for (const key of committedParameterValuesRef.current.keys()) {
                                if (key.startsWith(`${nodeId}:`)) {
                                        committedParameterValuesRef.current.delete(key);
                                }
                        }
                        parameters.forEach((parameter) => {
                                committedParameterValuesRef.current.set(`${nodeId}:${parameter.name}`, parameter.value);
                        });
                        setSelectedNodeParams(parameters);
                        setSelectedNodeMeta(graphNode);
                } catch (_) {
                        for (const key of committedParameterValuesRef.current.keys()) {
                                if (key.startsWith(`${nodeId}:`)) {
                                        committedParameterValuesRef.current.delete(key);
                                }
                        }
                        setSelectedNodeParams([]);
                        setSelectedNodeMeta(null);
                }
        }

        function updateDraftNodeSelection(nodeId) {
                if (!nodeId || liveNodeIds.has(nodeId)) {
                        return;
                }

                for (const key of committedParameterValuesRef.current.keys()) {
                        if (key.startsWith(`${nodeId}:`)) {
                                committedParameterValuesRef.current.delete(key);
                        }
                }

                const localNode = localDraftRuntimes.flatMap((runtime) => runtime.nodes || []).find((node) => node.id === nodeId);
                setSelectedNodeMeta(localNode || null);
                setSelectedNodeParams(localNode ? [
                        { name: 'type', type: 'string', value: localNode.type, description: 'Node type name' },
                        { name: 'runtimeTargetIp', type: 'string', value: '', description: 'Planned target runtime IP' }
                ] : []);
        }

        async function updateParameter(name, value) {
                const options = arguments.length > 2 ? arguments[2] : {};
                if (!selectedNodeId) return;

                const parameterType = options?.parameterType || '';
                const normalizeParameterValue = (inputValue) => {
                        if (parameterType === 'bool') {
                                if (typeof inputValue === 'boolean') {
                                        return inputValue;
                                }
                                const lowered = String(inputValue ?? '').trim().toLowerCase();
                                return lowered === 'true' || lowered === '1' || lowered === 'yes' || lowered === 'on';
                        }
                        if (parameterType === 'int') {
                                const parsed = Number.parseInt(String(inputValue ?? ''), 10);
                                return Number.isFinite(parsed) ? parsed : null;
                        }
                        if (parameterType === 'double') {
                                const parsed = Number.parseFloat(String(inputValue ?? ''));
                                return Number.isFinite(parsed) ? parsed : null;
                        }
                        return String(inputValue ?? '');
                };

                const nodeId = selectedNodeId;
                const interaction = options?.interaction || 'immediate';
                const hasSideEffects = Boolean(options?.hasSideEffects);
                const updateKey = `${nodeId}:${name}`;
                const currentParameter = selectedNodeParams.find((item) => item.name === name);
                const normalizedNextValue = normalizeParameterValue(value);

                if (!liveNodeIds.has(selectedNodeId)) {
                        const normalizedCurrentValue = normalizeParameterValue(currentParameter?.value);
                        if (normalizedNextValue === normalizedCurrentValue) {
                                return;
                        }
                        setSelectedNodeParams((current) => current.map((item) => (item.name === name ? { ...item, value } : item)));
                        return;
                }

                setSelectedNodeParams((current) => current.map((item) => (item.name === name ? { ...item, value } : item)));

                const activeTimer = pendingParameterUpdateTimersRef.current.get(updateKey);
                const committedRawValue = committedParameterValuesRef.current.has(updateKey)
                        ? committedParameterValuesRef.current.get(updateKey)
                        : currentParameter?.value;
                const normalizedCommittedValue = normalizeParameterValue(committedRawValue);
                if (normalizedNextValue === normalizedCommittedValue) {
                        if (activeTimer) {
                                window.clearTimeout(activeTimer);
                                pendingParameterUpdateTimersRef.current.delete(updateKey);
                        }
                        return;
                }

                const submitUpdate = async (nextValue) => {
                        try {
                                await updateNodeParameter(nodeId, name, nextValue);
                                setEditorError('');
                                committedParameterValuesRef.current.set(updateKey, nextValue);
                                if (hasSideEffects) {
                                        await fetchNodeParameters(nodeId);
                                }
                        } catch (error) {
                                const reason = error instanceof Error ? error.message : 'unknown error';
                                setEditorError(`failed to update ${name}: ${reason}`);
                        }
                };

                if (interaction === 'slider' && parameterType === 'int') {
                        if (activeTimer) {
                                window.clearTimeout(activeTimer);
                        }
                        const timerId = window.setTimeout(() => {
                                pendingParameterUpdateTimersRef.current.delete(updateKey);
                                void submitUpdate(value);
                        }, 200);
                        pendingParameterUpdateTimersRef.current.set(updateKey, timerId);
                        return;
                }

                if (activeTimer) {
                        window.clearTimeout(activeTimer);
                        pendingParameterUpdateTimersRef.current.delete(updateKey);
                }
                await submitUpdate(value);
        }

        async function setPipelineStopped(stopped) {
                if (stopped) {
                        // Optimistically stop local stream rendering immediately to avoid websocket backpressure while runtime stops.
                        setStatus('stopped');
                }
                try {
                        const state = await setRuntimeStopped(stopped);
                        setStatus(state.state === 'stopped' ? 'stopped' : 'running');
                        await fetchRuntimeStatus();
                } catch (_) {
                        void fetchRuntimeStatus();
                        setEditorError('runtime toggle failed');
                }
        }

        function onStartRuntime(runtimeId, runtimeState) {
                const runtime = editorGraph.runtimes.find((item) => item.id === runtimeId);
                if (!runtime) {
                        return;
                }
                const shouldStop = runtimeState === 'running';
                if (runtimeId === LOCAL_RUNTIME_ID) {
                        void setPipelineStopped(shouldStop);
                        return;
                }
                void toggleRemoteRuntime(runtime, shouldStop);
        }

        function processLatestFrame() {
                if (frameBinaryProcessingRef.current) return;
                frameBinaryProcessingRef.current = true;
                try {
                        while (latestFrameBinaryRef.current) {
                                const packet = parseBinaryFramePacket(latestFrameBinaryRef.current);
                                latestFrameBinaryRef.current = null;
                                if (!packet) continue;
                                if (expectedFrameSequenceRef.current >= 0 && packet.meta.sequence !== expectedFrameSequenceRef.current) {
                                        continue;
                                }
                                if (packet.meta.sequence <= lastRenderedSequenceRef.current) {
                                        continue;
                                }
                                if (!renderRawFrame(packet.meta, packet.bytes, false)) {
                                        setEditorError('frame conversion unsupported');
                                        continue;
                                }
                                // Keep the latest raw packet so UI-side controls can re-render
                                // immediately even when no fresh frame arrives.
                                lastFramePacketRef.current = {
                                        meta: packet.meta,
                                        bytes: new Uint8Array(packet.bytes)
                                };
                                lastRenderedSequenceRef.current = packet.meta.sequence;
                                expectedFrameSequenceRef.current = -1;
                                lastFrameSeenAtRef.current = Date.now();
                                setFrameMeta(packet.meta);
                                updateFps(packet.meta);
                        }
                } finally {
                        frameBinaryProcessingRef.current = false;
                }
        }

        function closeFrameSockets() {
                const streamSocket = frameSocketRef.current;
                if (streamSocket && (streamSocket.readyState === WebSocket.OPEN || streamSocket.readyState === WebSocket.CONNECTING)) {
                        streamSocket.close();
                }

                frameSocketRef.current = null;
                latestFrameBinaryRef.current = null;
                lastRenderedSequenceRef.current = -1;
                expectedFrameSequenceRef.current = -1;
        }

        function appendRuntimeLog(runtimeId, record) {
                if (!runtimeId || !record) {
                        return;
                }

                const recordSource = String(record?.source || 'runtime');
                const recordType = String(record?.type || 'info');
                const recordMessage = String(record?.message || record?.rendered || '');
                const recordTimestamp = Number(record?.timestampMs || 0);
                const dedupeKey = `${recordTimestamp}|${recordSource}|${recordType}|${recordMessage}`;

                let seenState = runtimeLogSeenRef.current.get(runtimeId);
                if (!seenState) {
                        seenState = { set: new Set(), order: [] };
                        runtimeLogSeenRef.current.set(runtimeId, seenState);
                }

                if (seenState.set.has(dedupeKey)) {
                        return;
                }

                seenState.set.add(dedupeKey);
                seenState.order.push(dedupeKey);
                while (seenState.order.length > 1200) {
                        const removed = seenState.order.shift();
                        if (removed) {
                                seenState.set.delete(removed);
                        }
                }

                setRuntimeLogs((current) => {
                        const existing = Array.isArray(current[runtimeId]) ? current[runtimeId] : [];
                        const duplicate = existing.slice(-120).some((entry) => {
                                const entrySource = String(entry?.source || 'runtime');
                                const entryType = String(entry?.type || 'info');
                                const entryMessage = String(entry?.message || entry?.rendered || '');
                                const entryTimestamp = Number(entry?.timestampMs || 0);
                                return entrySource === recordSource
                                        && entryType === recordType
                                        && entryMessage === recordMessage
                                        && entryTimestamp === recordTimestamp;
                        });

                        if (duplicate) {
                                return current;
                        }

                        const nextRecord = {
                                ...record,
                                __uiKey: `${runtimeId}|${dedupeKey}|${runtimeLogUiSeqRef.current++}`
                        };
                        const next = [...existing, nextRecord];
                        if (next.length > 300) {
                                next.splice(0, next.length - 300);
                        }
                        return { ...current, [runtimeId]: next };
                });
        }

        function onToggleRuntimeLogPanel(runtimeId, open) {
                setRuntimeLogPanels((current) => ({ ...current, [runtimeId]: Boolean(open) }));
        }

        function onClearRuntimeLogs(runtimeId) {
                if (!runtimeId) {
                        return;
                }
                runtimeLogSeenRef.current.delete(runtimeId);
                setRuntimeLogs((current) => ({ ...current, [runtimeId]: [] }));
        }

        useEffect(() => {
                const lastPacket = lastFramePacketRef.current;
                if (!lastPacket || !lastPacket.meta || !lastPacket.bytes) {
                        return;
                }

                // Recompute the displayed image immediately from the most recent raw frame
                // whenever local viewer controls change.
                renderRawFrame(lastPacket.meta, lastPacket.bytes, false);
                // eslint-disable-next-line react-hooks/exhaustive-deps
        }, [debayerEnabled]);

        useEffect(() => {
                return () => {
                        pendingParameterUpdateTimersRef.current.forEach((timerId) => window.clearTimeout(timerId));
                        pendingParameterUpdateTimersRef.current.clear();
                };
        }, []);

        useEffect(() => {
                pendingParameterUpdateTimersRef.current.forEach((timerId) => window.clearTimeout(timerId));
                pendingParameterUpdateTimersRef.current.clear();
        }, [selectedNodeId]);

        useEffect(() => {
                statusRef.current = status;
        }, [status]);

        useEffect(() => {
                selectedRuntimeIdRef.current = selectedRuntimeId;
        }, [selectedRuntimeId]);

        useEffect(() => {
                const keydownHandler = (event) => {
                        if (event.code === 'Escape' && !areKeyboardShortcutsBlocked()) {
                                setFilterCloseRequest((request) => request + 1);
                                return;
                        }
                        if (event.code === 'Tab' && !event.altKey && !event.ctrlKey && !event.metaKey) {
                                focusAdjacentParameter(event);
                                return;
                        }
                        if (focusParameterByTypeahead(event, parameterTypeaheadRef)) {
                                return;
                        }
                        if (event.defaultPrevented || event.repeat || !event.altKey || event.ctrlKey || event.metaKey || event.shiftKey || areKeyboardShortcutsBlocked()) {
                                return;
                        }
                        if (event.code === 'KeyV') {
                                event.preventDefault();
                                setViewMode((current) => current === 'viewer' ? 'editor' : 'viewer');
                                return;
                        }
                        if (event.code === 'KeyR') {
                                event.preventDefault();
                                void setPipelineStopped(statusRef.current === 'running');
                                return;
                        }
                        if (event.code === 'KeyH') {
                                event.preventDefault();
                                setShortcutPanelOpen(true);
                                return;
                        }
                        if (event.code === 'KeyF') {
                                event.preventDefault();
                                setParameterFilterOpenRequest((request) => request + 1);
                                return;
                        }
                        if (event.code === 'KeyK') {
                                event.preventDefault();
                                onClearRuntimeLogs(selectedRuntimeIdRef.current);
                                return;
                        }
                        if (event.code === 'KeyG') {
                                event.preventDefault();
                                const runtimeId = selectedRuntimeIdRef.current;
                                setRuntimeLogPanels((current) => ({ ...current, [runtimeId]: true }));
                                setRuntimeLogFilterOpenRequest((request) => ({ runtimeId, sequence: request.sequence + 1 }));
                        }
                };
                const pointerDownHandler = () => {
                        parameterTypeaheadRef.current = { text: '', timestamp: 0, focusedElement: null };
                };

                window.addEventListener('keydown', keydownHandler);
                document.addEventListener('pointerdown', pointerDownHandler);
                return () => {
                        window.removeEventListener('keydown', keydownHandler);
                        document.removeEventListener('pointerdown', pointerDownHandler);
                };
                // Register shortcuts once; current runtime state is read through statusRef.
                // eslint-disable-next-line react-hooks/exhaustive-deps
        }, []);

        useEffect(() => {
                void fetchRuntimeStatus();
                void fetchRuntimeVersion();
                void fetchPipeline();
                void fetchNodeCatalog();

                const healthTimer = setInterval(() => {
                        const currentStatus = statusRef.current;
                        if (currentStatus !== 'running' && currentStatus !== 'starting') {
                                return;
                        }
                        const streamRecentlyActive = statusRef.current === 'running' && Date.now() - lastFrameSeenAtRef.current < 3000;
                        if (!streamRecentlyActive) {
                                void fetchRuntimeStatus();
                        }
                }, 5000);

                return () => {
                        clearInterval(healthTimer);
                };
                // eslint-disable-next-line react-hooks/exhaustive-deps
        }, []);

        useEffect(() => {
                if (status !== 'running') {
                        closeFrameSockets();
                        return undefined;
                }

                const frameSocket = new WebSocket(wsUrl(WS_FRAME));
                frameSocket.binaryType = 'arraybuffer';
                frameSocketRef.current = frameSocket;
                frameSocket.onopen = () => {
                        const subscribeImageKey = selectedImageKey.includes('.') ? selectedImageKey : '';
                        frameSocket.send(JSON.stringify({ cmd: 'subscribe', nodeId: selectedNodeId, imageKey: subscribeImageKey }));
                };
                frameSocket.onmessage = (event) => {
                        if (typeof event.data === 'string') {
                                try {
                                        const data = JSON.parse(event.data);
                                        setFrameContextState(data);

                                        const imageList = Array.isArray(data.images) ? data.images : [];
                                        if (imageList.length > 0) {
                                                const preferredImage = imageList.find((img) => img.key === selectedImageKey) || imageList[0];
                                                const nextImageKey = preferredImage?.key || selectedImageKey;
                                                if (nextImageKey !== selectedImageKey) {
                                                        setSelectedImageKey(nextImageKey);
                                                }
                                                if (preferredImage && Number.isFinite(preferredImage.sequence)) {
                                                        expectedFrameSequenceRef.current = preferredImage.sequence;
                                                }
                                        }
                                } catch (_) {
                                        setEditorError('invalid framecontext websocket payload');
                                }
                                return;
                        }

                        if (event.data instanceof ArrayBuffer) {
                                latestFrameBinaryRef.current = event.data;
                                processLatestFrame();
                        }
                };

                return () => {
                        if (frameSocketRef.current === frameSocket) {
                                frameSocketRef.current = null;
                        }
                        frameSocket.close();
                };
                // eslint-disable-next-line react-hooks/exhaustive-deps
        }, [status]);

        useEffect(() => {
                const sockets = runtimeLogSocketRef.current;
                const desired = new Map();

                for (const target of runtimeLogTargets) {
                        desired.set(target.runtimeId, target.socketUrl);

                        const existing = sockets.get(target.runtimeId);
                        if (existing && existing.socketUrl === target.socketUrl && (existing.socket.readyState === WebSocket.OPEN || existing.socket.readyState === WebSocket.CONNECTING)) {
                                continue;
                        }

                        if (existing) {
                                existing.socket.close();
                                sockets.delete(target.runtimeId);
                        }

                        const logSocket = new WebSocket(target.socketUrl);
                        const connection = { socket: logSocket, socketUrl: target.socketUrl };
                        sockets.set(target.runtimeId, connection);

                        logSocket.onmessage = (event) => {
                                if (typeof event.data !== 'string') {
                                        return;
                                }
                                try {
                                        appendRuntimeLog(target.runtimeId, JSON.parse(event.data));
                                } catch (_) {
                                        // Ignore malformed log payloads.
                                }
                        };

                        logSocket.onclose = () => {
                                const current = sockets.get(target.runtimeId);
                                if (current && current.socket === logSocket) {
                                        sockets.delete(target.runtimeId);
                                }
                        };

                        logSocket.onerror = () => {
                                const current = sockets.get(target.runtimeId);
                                if (current && current.socket === logSocket) {
                                        current.socket.close();
                                }
                        };
                }

                for (const [runtimeId, connection] of sockets.entries()) {
                        if (!desired.has(runtimeId)) {
                                connection.socket.close();
                                sockets.delete(runtimeId);
                        }
                }
                // eslint-disable-next-line react-hooks/exhaustive-deps
        }, [runtimeLogTargetsSignature]);

        useEffect(() => {
                return () => {
                        for (const connection of runtimeLogSocketRef.current.values()) {
                                connection.socket.close();
                        }
                        runtimeLogSocketRef.current.clear();
                };
        }, []);

        useEffect(() => {
                const socket = frameSocketRef.current;
                if (socket && socket.readyState === WebSocket.OPEN) {
                        const subscribeImageKey = selectedImageKey.includes('.') ? selectedImageKey : '';
                        socket.send(JSON.stringify({ cmd: 'subscribe', nodeId: selectedNodeId, imageKey: subscribeImageKey }));
                }
                lastRenderedSequenceRef.current = -1;
                expectedFrameSequenceRef.current = -1;
                // eslint-disable-next-line react-hooks/exhaustive-deps
        }, [selectedNodeId]);

        useEffect(() => {
                if (suppressNextParameterReloadRef.current) {
                        suppressNextParameterReloadRef.current = false;
                        return;
                }
                if (!selectedNodeId) {
                        setSelectedNodeMeta(null);
                        setSelectedNodeParams([]);
                        return;
                }

                if (liveNodeIds.has(selectedNodeId)) {
                        void fetchNodeParameters(selectedNodeId);
                        return;
                }

                updateDraftNodeSelection(selectedNodeId);
                // eslint-disable-next-line react-hooks/exhaustive-deps
        }, [selectedNodeId]);

        useEffect(() => {
                if (!selectedNodeId || liveNodeIds.has(selectedNodeId)) {
                        return;
                }
                updateDraftNodeSelection(selectedNodeId);
                // eslint-disable-next-line react-hooks/exhaustive-deps
        }, [localDraftRuntimes, selectedNodeId]);

        useEffect(() => {
                const socket = frameSocketRef.current;
                if (socket && socket.readyState === WebSocket.OPEN) {
                        const subscribeImageKey = selectedImageKey.includes('.') ? selectedImageKey : '';
                        socket.send(JSON.stringify({ cmd: 'subscribe', nodeId: selectedNodeId, imageKey: subscribeImageKey }));
                }
                // eslint-disable-next-line react-hooks/exhaustive-deps
        }, [selectedImageKey]);

        useEffect(() => {
                const runtime = editorGraph.runtimes.find((item) => item.id === selectedRuntimeId);
                if (runtime) {
                        setSelectedRuntimeName(runtimeDisplayLabel(runtime, localIp));
                }
        }, [editorGraph.runtimes, selectedRuntimeId, localIp]);

        useEffect(() => {
                const viewport = frameViewportRef.current;
                if (!viewport || typeof ResizeObserver === 'undefined') {
                        return undefined;
                }

                const observer = new ResizeObserver(updateFrameViewportSize);
                observer.observe(viewport);
                return () => observer.disconnect();
                // The observer remains attached to the stable frame viewport element.
                // eslint-disable-next-line react-hooks/exhaustive-deps
        }, []);

        useLayoutEffect(() => {
                updateFrameViewportSize();
                // Recalculate synchronously after the mode's layout classes are committed.
                // eslint-disable-next-line react-hooks/exhaustive-deps
        }, [viewMode]);

        useEffect(() => {
                const canvas = frameCanvasRef.current;
                const viewport = frameViewportRef.current;
                if (!canvas || !viewport) return;
                const baseScale = Math.min((viewport.clientWidth || 1) / (canvas.width || 1), (viewport.clientHeight || 1) / (canvas.height || 1));
                frameViewportScaleRef.current = baseScale;
                const scale = baseScale * viewZoom;
                const scaledWidth = (canvas.width || 1) * scale;
                const scaledHeight = (canvas.height || 1) * scale;
                const baseX = ((viewport.clientWidth || 1) - scaledWidth) / 2;
                const baseY = ((viewport.clientHeight || 1) - scaledHeight) / 2;
                canvas.style.transform = `translate(${baseX + viewPanX}px, ${baseY + viewPanY}px) scale(${scale})`;
        }, [viewZoom, viewPanX, viewPanY, frameMeta, frameViewportSize]);

        useEffect(() => {
                setRuntimeLayouts((current) => {
                        const next = { ...current };
                        let changed = false;
                        editorGraph.runtimes.forEach((runtime, index) => {
                                if (!next[runtime.id]) {
                                        next[runtime.id] = runtimeDefaultRect(index);
                                        changed = true;
                                }
                        });
                        return changed ? next : current;
                });
                setNodeLayouts((current) => {
                        const next = { ...current };
                        let changed = false;
                        editorGraph.runtimes.forEach((runtime) => {
                                runtime.nodes.forEach((node, index) => {
                                        if (!next[node.id]) {
                                                next[node.id] = nodeDefaultPos(index);
                                                changed = true;
                                        }
                                });
                        });
                        return changed ? next : current;
                });
        }, [editorGraph.runtimes]);

        useEffect(() => {
                if (hasAutoCenteredEditorRef.current || editorGraph.nodeIds.length === 0) {
                        return undefined;
                }
                const timerId = window.setTimeout(() => {
                        if (fitEditorView()) {
                                hasAutoCenteredEditorRef.current = true;
                        }
                }, 0);
                return () => window.clearTimeout(timerId);
                // Center only after the first discovered graph has reached the DOM.
                // eslint-disable-next-line react-hooks/exhaustive-deps
        }, [editorNodeIdsSignature, viewMode]);

        useEffect(() => {
                const moveHandler = (event) => {
                        const gesture = viewerPanGestureRef.current;
                        const buttonMask = gesture.button === 1 ? 4 : gesture.button === 2 ? 2 : 0;
                        if (!isPanning || !buttonMask || (event.buttons & buttonMask) === 0) return;
                        if (gesture.active && !gesture.moved) {
                                const dx = event.clientX - gesture.startX;
                                const dy = event.clientY - gesture.startY;
                                if ((dx * dx + dy * dy) >= PAN_DRAG_THRESHOLD * PAN_DRAG_THRESHOLD) {
                                        gesture.moved = true;
                                }
                        }
                        if (!gesture.moved) return;
                        setViewPanX(panOrigin.baseX + (event.clientX - panOrigin.x));
                        setViewPanY(panOrigin.baseY + (event.clientY - panOrigin.y));
                };
                const upHandler = () => {
                        setIsPanning(false);
                        viewerPanGestureRef.current.active = false;
                };
                window.addEventListener('mousemove', moveHandler);
                window.addEventListener('mouseup', upHandler);
                window.addEventListener('blur', upHandler);
                return () => {
                        window.removeEventListener('mousemove', moveHandler);
                        window.removeEventListener('mouseup', upHandler);
                        window.removeEventListener('blur', upHandler);
                };
        }, [isPanning, panOrigin]);

        useEffect(() => {
                const moveHandler = (event) => {
                        const gesture = editorPanGestureRef.current;
                        const buttonMask = gesture.button === 1 ? 4 : gesture.button === 2 ? 2 : 0;
                        if (!editorPanning || !buttonMask || (event.buttons & buttonMask) === 0) return;
                        if (gesture.active && !gesture.moved) {
                                const dx = event.clientX - gesture.startX;
                                const dy = event.clientY - gesture.startY;
                                if ((dx * dx + dy * dy) >= PAN_DRAG_THRESHOLD * PAN_DRAG_THRESHOLD) {
                                        gesture.moved = true;
                                }
                        }
                        if (!gesture.moved) return;
                        setEditorPanX(editorPanOrigin.baseX + (event.clientX - editorPanOrigin.x));
                        setEditorPanY(editorPanOrigin.baseY + (event.clientY - editorPanOrigin.y));
                };
                const upHandler = () => {
                        setEditorPanning(false);
                        editorPanGestureRef.current.active = false;
                };
                window.addEventListener('mousemove', moveHandler);
                window.addEventListener('mouseup', upHandler);
                window.addEventListener('blur', upHandler);
                return () => {
                        window.removeEventListener('mousemove', moveHandler);
                        window.removeEventListener('mouseup', upHandler);
                        window.removeEventListener('blur', upHandler);
                };
        }, [editorPanning, editorPanOrigin]);

        useEffect(() => {
                if (!dragState) {
                        return undefined;
                }

                const moveHandler = (event) => {
                        if (event.buttons === 0) {
                                setDragState(null);
                                return;
                        }

                        const dx = (event.clientX - dragState.startX) / Math.max(editorZoom, 0.0001);
                        const dy = (event.clientY - dragState.startY) / Math.max(editorZoom, 0.0001);

                        if (dragState.kind === 'runtime') {
                                const nextRect = {
                                        ...dragState.startRect,
                                        x: dragState.startRect.x + dx,
                                        y: dragState.startRect.y + dy
                                };
                                if (!canPlaceRuntime(dragState.runtimeId, nextRect)) {
                                        return;
                                }
                                setRuntimeLayouts((current) => ({ ...current, [dragState.runtimeId]: nextRect }));
                                return;
                        }

                        if (dragState.kind === 'runtime-resize') {
                                const nextRect = {
                                        ...dragState.startRect,
                                        w: Math.max(RUNTIME_MIN_WIDTH, dragState.startRect.w + dx),
                                        h: Math.max(RUNTIME_MIN_HEIGHT, dragState.startRect.h + dy)
                                };
                                if (!canPlaceRuntime(dragState.runtimeId, nextRect)) {
                                        return;
                                }
                                setRuntimeLayouts((current) => ({ ...current, [dragState.runtimeId]: nextRect }));
                                return;
                        }

                        if (dragState.kind === 'node') {
                                suppressNextNodeSelectRef.current = true;
                                const nextPos = {
                                        x: dragState.startPos.x + dx / Math.max(dragState.runtimeZoom || 1, 0.0001),
                                        y: dragState.startPos.y + dy / Math.max(dragState.runtimeZoom || 1, 0.0001)
                                };
                                setNodeLayouts((current) => ({ ...current, [dragState.nodeId]: nextPos }));
                        }
                };

                const upHandler = () => setDragState(null);
                const cancelHandler = () => setDragState(null);

                window.addEventListener('mousemove', moveHandler);
                window.addEventListener('mouseup', upHandler);
                window.addEventListener('blur', cancelHandler);
                document.addEventListener('mouseleave', cancelHandler);
                return () => {
                        window.removeEventListener('mousemove', moveHandler);
                        window.removeEventListener('mouseup', upHandler);
                        window.removeEventListener('blur', cancelHandler);
                        document.removeEventListener('mouseleave', cancelHandler);
                };
        }, [dragState, editorGraph.runtimes, editorZoom]);

        useEffect(() => {
                if (!splitDragState) {
                        return undefined;
                }

                const moveHandler = (event) => {
                        const layout = mainLayoutRef.current;
                        if (!layout) {
                                return;
                        }
                        const dx = event.clientX - splitDragState.startX;
                        const ratioShift = dx / Math.max(layout.clientWidth, 1);
                        const nextRatio = Math.max(0.42, Math.min(0.82, splitDragState.startRatio + ratioShift));
                        setSplitRatio(nextRatio);
                };

                const stopHandler = () => setSplitDragState(null);

                window.addEventListener('mousemove', moveHandler);
                window.addEventListener('mouseup', stopHandler);
                window.addEventListener('blur', stopHandler);
                return () => {
                        window.removeEventListener('mousemove', moveHandler);
                        window.removeEventListener('mouseup', stopHandler);
                        window.removeEventListener('blur', stopHandler);
                };
        }, [splitDragState]);

        const images = Array.isArray(frameContextState?.images) ? frameContextState.images : [];
        const versionParts = splitVersionText(versionText);
        const frameAspectRatio = frameMeta?.width && frameMeta?.height ? `${frameMeta.width} / ${frameMeta.height}` : '4 / 3';
        const hasFrame = Boolean(frameMeta && Number.isFinite(frameMeta.sequence));
        const seqText = hasFrame ? `#${frameMeta.sequence}` : '#-';
        const tsMs = hasFrame ? Math.round((frameMeta.timestampNs || 0) / 1_000_000) : '-';
        const captureText = hasFrame ? captureFps.toFixed(1) : '-';
        const renderText = hasFrame ? renderFps.toFixed(1) : '-';
        const pipelineStopped = status === 'stopped';
        const runtimeDown = status === 'down';
        const runtimeStatusText = runtimeDown ? 'down' : (pipelineStopped ? 'stopped' : 'running');
        const runtimeRunning = status === 'running';
        const runtimeBounds = editorGraph.runtimes.reduce((acc, runtime) => {
                const rect = runtime.rect || { x: 0, y: 0, w: 0, h: 0 };
                acc.minX = Math.min(acc.minX, rect.x);
                acc.minY = Math.min(acc.minY, rect.y);
                acc.maxX = Math.max(acc.maxX, rect.x + rect.w);
                acc.maxY = Math.max(acc.maxY, rect.y + rect.h);
                return acc;
        }, { minX: 0, minY: 0, maxX: 960, maxY: 640 });
        const canvasWidth = Math.max(960, Math.ceil(runtimeBounds.maxX + 240));
        const canvasHeight = Math.max(640, Math.ceil(runtimeBounds.maxY + 240));

        const absoluteCrossEdges = editorGraph.crossRuntimeEdges.map((edge) => {
                const fromRuntime = editorGraph.runtimes.find((runtime) => runtime.id === edge.fromRuntime);
                const toRuntime = editorGraph.runtimes.find((runtime) => runtime.id === edge.toRuntime);
                if (!fromRuntime || !toRuntime) {
                        return null;
                }
                const fromNode = fromRuntime.nodes.find((node) => node.id === edge.fromNode);
                const toNode = toRuntime.nodes.find((node) => node.id === edge.toNode);
                if (!fromNode || !toNode) {
                        return null;
                }
                const fromViewport = runtimeViewports[fromRuntime.id] || DEFAULT_RUNTIME_VIEWPORT;
                const toViewport = runtimeViewports[toRuntime.id] || DEFAULT_RUNTIME_VIEWPORT;
                return {
                        id: edge.id,
                        fromNode: edge.fromNode,
                        fromPort: edge.fromPort,
                        toNode: edge.toNode,
                        toPort: edge.toPort,
                        from: {
                                ...fromNode,
                                x: fromRuntime.rect.x + fromViewport.panX + fromNode.x * fromViewport.zoom,
                                y: fromRuntime.rect.y + RUNTIME_HEADER_HEIGHT + fromViewport.panY + fromNode.y * fromViewport.zoom,
                                scale: fromViewport.zoom
                        },
                        to: {
                                ...toNode,
                                x: toRuntime.rect.x + toViewport.panX + toNode.x * toViewport.zoom,
                                y: toRuntime.rect.y + RUNTIME_HEADER_HEIGHT + toViewport.panY + toNode.y * toViewport.zoom,
                                scale: toViewport.zoom
                        }
                };
        }).filter(Boolean);

        return (
                <div className="app-shell" onClick={closeMenu}>
                        <GlobalHeader
                                runtimeStatusText={runtimeStatusText}
                                versionParts={versionParts}
                                graphStatusText={editorError || graphStatusText}
                                statusError={Boolean(editorError)}
                                runtimeRunning={runtimeRunning}
                                onToggleRuntime={() => void setPipelineStopped(runtimeRunning)}
                                viewMode={viewMode}
                                onSetViewMode={setViewMode}
                                shortcutPanelOpen={shortcutPanelOpen}
                                onSetShortcutPanelOpen={setShortcutPanelOpen}
                        />

                        <main
                                ref={mainLayoutRef}
                                className={`layout ${viewMode === 'editor' ? 'mode-editor' : 'mode-viewer'}`}
                                style={{
                                        '--left-pane-width': `${(splitRatio * 100).toFixed(1)}%`,
                                        '--left-pane-ratio': splitRatio
                                }}
                                onClick={(event) => event.stopPropagation()}
                        >
                                <NodeEditorPanel
                                        viewMode={viewMode}
                                        editorViewportRef={editorViewportRef}
                                        onOpenBackgroundMenu={(event) => {
                                                if (event.button === 2) {
                                                        if (editorPanGestureRef.current.moved) {
                                                                event.preventDefault();
                                                                event.stopPropagation();
                                                        } else {
                                                                openMenu(event, 'background');
                                                        }
                                                        editorPanGestureRef.current = { active: false, moved: false, startX: 0, startY: 0, button: null };
                                                        return;
                                                }
                                                openMenu(event, 'background');
                                        }}
                                        onWheelCapture={(event) => {
                                                const target = event.target instanceof Element ? event.target : null;
                                                if (target && target.closest('.runtime-canvas,.runtime-log-console,.runtime-log-filter-panel,.runtime-log-scroll-area,.runtime-log-search-field')) {
                                                        return;
                                                }

                                                event.preventDefault();
                                                event.stopPropagation();

                                                // Shift/Alt + wheel pans the editor; plain wheel zooms at cursor.
                                                if (event.shiftKey || event.altKey) {
                                                        if (event.shiftKey) {
                                                                setEditorPanX((current) => current - event.deltaY);
                                                        }
                                                        if (event.altKey || (!event.shiftKey && Math.abs(event.deltaX) > 0)) {
                                                                const delta = event.altKey ? event.deltaY : event.deltaX;
                                                                setEditorPanY((current) => current - delta);
                                                        }
                                                        return;
                                                }

                                                const viewport = editorViewportRef.current;
                                                if (!viewport) {
                                                        return;
                                                }
                                                const rect = viewport.getBoundingClientRect();
                                                const cursorX = event.clientX - rect.left;
                                                const cursorY = event.clientY - rect.top;
                                                const factor = Math.exp(-event.deltaY * 0.0016);
                                                setEditorZoom((currentZoom) => {
                                                        const nextZoom = Math.max(EDITOR_MIN_ZOOM, Math.min(EDITOR_MAX_ZOOM, currentZoom * factor));
                                                        if (nextZoom === currentZoom) {
                                                                return currentZoom;
                                                        }
                                                        const graphX = (cursorX - editorPanX) / currentZoom;
                                                        const graphY = (cursorY - editorPanY) / currentZoom;
                                                        setEditorPanX(cursorX - graphX * nextZoom);
                                                        setEditorPanY(cursorY - graphY * nextZoom);
                                                        return nextZoom;
                                                });
                                        }}
                                        editorZoom={editorZoom}
                                        editorPanX={editorPanX}
                                        editorPanY={editorPanY}
                                        onResetView={fitEditorView}
                                        canvasWidth={canvasWidth}
                                        canvasHeight={canvasHeight}
                                        onPanMouseDown={(event) => {
                                                if (event.button !== 1 && event.button !== 2) {
                                                        return;
                                                }
                                                event.preventDefault();
                                                if (event.button === 2) {
                                                        event.stopPropagation();
                                                }
                                                editorPanGestureRef.current = {
                                                        active: true,
                                                        moved: false,
                                                        startX: event.clientX,
                                                        startY: event.clientY,
                                                        button: event.button
                                                };
                                                setEditorPanning(true);
                                                setEditorPanOrigin({ x: event.clientX, y: event.clientY, baseX: editorPanX, baseY: editorPanY });
                                        }}
                                        absoluteCrossEdges={absoluteCrossEdges}
                                        absoluteEdgeCurvePath={absoluteEdgeCurvePath}
                                        onDeleteEdgeByText={deleteEdgeByText}
                                        editorGraph={editorGraph}
                                        localIp={localIp}
                                        renameRuntime={renameRuntime}
                                        renameNode={renameNode}
                                        setNodePortVisibility={setNodePortVisibility}
                                        selectedNodeId={selectedNodeId}
                                        suppressNextNodeSelectRef={suppressNextNodeSelectRef}
                                        connectNodes={connectNodes}
                                        setSelectedNodeId={setSelectedNodeId}
                                        openMenu={openMenu}
                                        setSelectedRuntimeId={setSelectedRuntimeId}
                                        edgeCurvePath={edgeCurvePath}
                                        startNodeDrag={startNodeDrag}
                                        startRuntimeDrag={startRuntimeDrag}
                                        startRuntimeResize={startRuntimeResize}
                                        onStartRuntime={onStartRuntime}
                                        runtimeDisplayLabel={runtimeDisplayLabel}
                                        runtimeLogs={runtimeLogs}
                                        runtimeLogPanels={runtimeLogPanels}
                                        onToggleRuntimeLogPanel={onToggleRuntimeLogPanel}
                                        onClearRuntimeLogs={onClearRuntimeLogs}
                                        runtimeLogFilterOpenRequest={runtimeLogFilterOpenRequest}
                                        filterCloseRequest={filterCloseRequest}
                                        runtimeBaseUrl={runtimeBaseUrl}
                                        selectedMediaElement={selectedMediaElement}
                                        onSelectMediaElement={setSelectedMediaElement}
                                        runtimeViewports={runtimeViewports}
                                        onRuntimeViewportChange={updateRuntimeViewport}
                                />

                                <div
                                        className="layout-splitter"
                                        role="separator"
                                        aria-label="Resize layout"
                                        aria-orientation="vertical"
                                        onMouseDown={(event) => {
                                                if (window.innerWidth <= 980) {
                                                        return;
                                                }
                                                event.preventDefault();
                                                event.stopPropagation();
                                                setSplitDragState({ startX: event.clientX, startRatio: splitRatio });
                                        }}
                                />

                                <FrameViewerPanel
                                        viewMode={viewMode}
                                        frameViewportRef={frameViewportRef}
                                        frameAspectRatio={frameAspectRatio}
                                        onWheelCapture={(event) => {
                                                event.preventDefault();
                                                event.stopPropagation();

                                                if (event.shiftKey || event.altKey) {
                                                        if (event.shiftKey) {
                                                                setViewPanX((current) => current - event.deltaY);
                                                        }
                                                        if (event.altKey || (!event.shiftKey && Math.abs(event.deltaX) > 0)) {
                                                                const delta = event.altKey ? event.deltaY : event.deltaX;
                                                                setViewPanY((current) => current - delta);
                                                        }
                                                        return;
                                                }

                                                const viewport = frameViewportRef.current;
                                                const canvas = frameCanvasRef.current;
                                                if (!viewport || !canvas?.width || !canvas?.height) return;

                                                const rect = viewport.getBoundingClientRect();
                                                const cursorX = event.clientX - rect.left;
                                                const cursorY = event.clientY - rect.top;
                                                const factor = Math.exp(-event.deltaY * 0.0019);

                                                setViewZoom((currentZoom) => {
                                                        const nextZoom = Math.min(24, Math.max(1, currentZoom * factor));
                                                        if (nextZoom === currentZoom) {
                                                                return currentZoom;
                                                        }

                                                        const baseScale = Math.min((viewport.clientWidth || 1) / (canvas.width || 1), (viewport.clientHeight || 1) / (canvas.height || 1));
                                                        const currentScale = baseScale * currentZoom;
                                                        const currentBaseX = ((viewport.clientWidth || 1) - (canvas.width * currentScale)) / 2;
                                                        const currentBaseY = ((viewport.clientHeight || 1) - (canvas.height * currentScale)) / 2;
                                                        const currentTotalX = currentBaseX + viewPanX;
                                                        const currentTotalY = currentBaseY + viewPanY;

                                                        const nextScale = baseScale * nextZoom;
                                                        const nextBaseX = ((viewport.clientWidth || 1) - (canvas.width * nextScale)) / 2;
                                                        const nextBaseY = ((viewport.clientHeight || 1) - (canvas.height * nextScale)) / 2;
                                                        const worldX = (cursorX - currentTotalX) / currentScale;
                                                        const worldY = (cursorY - currentTotalY) / currentScale;

                                                        const nextTotalX = cursorX - worldX * nextScale;
                                                        const nextTotalY = cursorY - worldY * nextScale;

                                                        setViewPanX(nextTotalX - nextBaseX);
                                                        setViewPanY(nextTotalY - nextBaseY);
                                                        return nextZoom;
                                                });
                                        }}
                                        seqText={seqText}
                                        tsMs={tsMs}
                                        captureText={captureText}
                                        renderText={renderText}
                                        hasFrame={hasFrame}
                                        debayerEnabled={debayerEnabled}
                                        setDebayerEnabled={updateDebayerEnabled}
                                        onResetView={() => { setViewZoom(1); setViewPanX(0); setViewPanY(0); }}
                                        status={status}
                                        frameCanvasRef={frameCanvasRef}
                                        onCanvasMouseDown={(event) => {
                                                if (event.button !== 1 && event.button !== 2) return;
                                                event.preventDefault();
                                                viewerPanGestureRef.current = {
                                                        active: true,
                                                        moved: false,
                                                        startX: event.clientX,
                                                        startY: event.clientY,
                                                        button: event.button
                                                };
                                                setIsPanning(true);
                                                setPanOrigin({ x: event.clientX, y: event.clientY, baseX: viewPanX, baseY: viewPanY });
                                        }}
                                        frameMeta={frameMeta}
                                        frameContextState={frameContextState}
                                        displayedFrameNodeName={displayedFrameNodeName}
                                        formatLabel={formatLabel}
                                />

                                <ParameterPanel
                                        selectedNodeMeta={selectedNodeMeta}
                                        selectedRuntimeName={selectedRuntimeName}
                                        onReload={() => {
                                                void fetchNodeParameters(selectedNodeId);
                                        }}
                                        selectedRuntimeId={selectedRuntimeId}
                                        localRuntimeId={LOCAL_RUNTIME_ID}
                                        deleteRuntime={deleteRuntime}
                                        selectedNodeParams={selectedNodeParams}
                                        updateParameter={updateParameter}
                                        runtimeRunning={runtimeRunning}
                                        selectedMediaElement={selectedMediaElement}
                                        filterOpenRequest={parameterFilterOpenRequest}
                                        filterCloseRequest={filterCloseRequest}
                                />
                        </main>

                        {dialogState.open && dialogState.mode === 'runtime' ? (
                                <div className="dialog-backdrop" onClick={() => setDialogState({ open: false, mode: null, runtimeId: null, runtimeName: '', runtimeIp: '', nodeType: '', nodeId: '' })}>
                                        <div className="dialog" onClick={(event) => event.stopPropagation()}>
                                                <h3>New Runtime</h3>
                                                <label>
                                                        Runtime Name
                                                        <Input
                                                                type="text"
                                                                value={dialogState.runtimeName}
                                                                onChange={(event) => setDialogState((current) => ({ ...current, runtimeName: event.target.value }))}
                                                        />
                                                </label>
                                                <label>
                                                        Runtime IP
                                                        <Input
                                                                type="text"
                                                                value={dialogState.runtimeIp}
                                                                onChange={(event) => setDialogState((current) => ({ ...current, runtimeIp: event.target.value }))}
                                                        />
                                                </label>
                                                <div className="dialog-actions">
                                                        <Button className="secondary" variant="secondary" type="button" onClick={() => setDialogState({ open: false, mode: null, runtimeId: null, runtimeName: '', runtimeIp: '', nodeType: '', nodeId: '' })}>Cancel</Button>
                                                        <Button type="button" onClick={addRuntime}>Create</Button>
                                                </div>
                                        </div>
                                </div>
                        ) : null}

                        <ContextMenu
                                menu={runtimeMenu}
                                onClose={closeMenu}
                                onAddRuntime={openRuntimeDialog}
                                onAddNode={(runtimeId, type) => openNodeDialog(runtimeId || selectedRuntimeId, type)}
                                onDeleteRuntime={(runtimeId) => void deleteRuntime(runtimeId)}
                                onDeleteNode={(nodeId) => void deleteNode(nodeId)}
                                onHidePort={(nodeId, direction, portName) => void hideNodePort(nodeId, direction, portName)}
                                localRuntimeId={LOCAL_RUNTIME_ID}
                        />
                </div>
        );
}