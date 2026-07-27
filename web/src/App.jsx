import React, { useEffect, useMemo, useRef, useState } from 'react';
import GlobalHeader from './features/header/GlobalHeader.jsx';
import NodeEditorPanel from './features/node-editor/NodeEditorPanel.jsx';
import ContextMenu from './features/node-editor/ContextMenu.jsx';
import FrameViewerPanel from './features/frame-context-viewer/FrameViewerPanel.jsx';
import ParameterPanel from './features/parameter-browser/ParameterPanel.jsx';
import UiButton from './components/UiButton.jsx';
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
        savePipeline,
        setRuntimeStopped,
        updateNodeParameter
} from './services/runtimeApi.js';
import { formatLabel, parseBinaryFramePacket, renderPacketToRgba } from './services/frameRendering.js';

const WS_FRAME = '/ws/frame';
const LOCAL_RUNTIME_ID = 'local';

const FALLBACK_NODE_TYPES = {
        sources: ['v4l2src', 'filesrc', 'nvargussrc'],
        processors: ['bitshift', 'debayer', 'ccm', 'compositor'],
        probes: ['probe'],
        sinks: ['filesink', 'tcpsink']
};

const DEFAULT_MODE = 'viewer';
const VIEW_MODE_STORAGE_KEY = 'camflow:view-mode';
const SELECTED_NODE_STORAGE_KEY = 'camflow:selected-node-id';
const RUNTIME_LAYOUTS_STORAGE_KEY = 'camflow:runtime-layouts';
const NODE_LAYOUTS_STORAGE_KEY = 'camflow:node-layouts';
const AUTO_NODE_PREFIX = '__auto__';
const NODE_WIDTH = 152;
const NODE_HEIGHT = 62;
const RUNTIME_HEADER_HEIGHT = 25;
const RUNTIME_MIN_WIDTH = 190;
const RUNTIME_MIN_HEIGHT = 120;
const EDITOR_MIN_ZOOM = 0.35;
const EDITOR_MAX_ZOOM = 2.25;
const DEFAULT_RUNTIME_API_PORT = '8000';

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

function nodeSideCenters(node) {
        return [
                { side: 'left', x: node.x, y: node.y + NODE_HEIGHT * 0.5, dx: -1, dy: 0 },
                { side: 'right', x: node.x + NODE_WIDTH, y: node.y + NODE_HEIGHT * 0.5, dx: 1, dy: 0 },
                { side: 'top', x: node.x + NODE_WIDTH * 0.5, y: node.y, dx: 0, dy: -1 },
                { side: 'bottom', x: node.x + NODE_WIDTH * 0.5, y: node.y + NODE_HEIGHT, dx: 0, dy: 1 }
        ];
}

function nearestSideAnchors(fromNode, toNode) {
        const fromSides = nodeSideCenters(fromNode);
        const toSides = nodeSideCenters(toNode);
        let best = null;

        for (const from of fromSides) {
                for (const to of toSides) {
                        const dx = to.x - from.x;
                        const dy = to.y - from.y;
                        const distanceSq = dx * dx + dy * dy;
                        if (!best || distanceSq < best.distanceSq) {
                                best = { from, to, distanceSq };
                        }
                }
        }

        return best;
}

function routedEdgeCurvePath(fromNode, toNode) {
        const anchors = nearestSideAnchors(fromNode, toNode);
        if (!anchors) {
                return '';
        }

        const x1 = anchors.from.x;
        const y1 = anchors.from.y;
        const x2 = anchors.to.x;
        const y2 = anchors.to.y;
        const distance = Math.hypot(x2 - x1, y2 - y1);
        const control = Math.max(34, Math.min(190, distance * 0.38));

        const c1x = x1 + anchors.from.dx * control;
        const c1y = y1 + anchors.from.dy * control;
        const c2x = x2 + anchors.to.dx * control;
        const c2y = y2 + anchors.to.dy * control;

        return `M ${x1} ${y1} C ${c1x} ${c1y}, ${c2x} ${c2y}, ${x2} ${y2}`;
}

function absoluteEdgeCurvePath(fromNode, toNode) {
        return routedEdgeCurvePath(fromNode, toNode);
}

function buildEditorGraph(pipeline, localDraftRuntimes, localIp, runtimeLayouts, nodeLayouts, runtimeStatus, remoteRuntimeStatuses) {
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
                runtime.nodes.push({ id: node.id, type: node.type || 'node', x: layoutPos?.x ?? defaultPos.x, y: layoutPos?.y ?? defaultPos.y, live: true });
        });

        (Array.isArray(localDraftRuntimes) ? localDraftRuntimes : []).forEach((runtime) => {
                const target = ensureRuntime(runtime.id, runtime.name, runtime.ip);
                (runtime.nodes || []).forEach((node, index) => {
                        if (isAutoNodeId(node.id) || liveNodeIds.has(node.id)) {
                                return;
                        }
                        const defaultPos = nodeDefaultPos(index);
                        const layoutPos = nodeLayouts?.[node.id];
                        target.nodes.push({ ...node, x: layoutPos?.x ?? node.x ?? defaultPos.x, y: layoutPos?.y ?? node.y ?? defaultPos.y });
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

        const runtimes = Array.from(runtimeMap.values()).filter((runtime) => runtime.nodes.length > 0 || runtime.id === LOCAL_RUNTIME_ID || draftRuntimeIds.has(runtime.id)).map((runtime, index) => {
                const defaultRect = runtimeDefaultRect(index);
                const layoutRect = runtimeLayouts?.[runtime.id];
                const isLocalRuntime = runtime.id === LOCAL_RUNTIME_ID;
                const runtimeState = isLocalRuntime ? (runtimeStatus || 'down') : (remoteRuntimeStatuses?.[runtime.id] || 'unknown');
                return {
                        ...runtime,
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

function edgeCurvePath(fromNode, toNode) {
        return routedEdgeCurvePath(fromNode, toNode);
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
        const [currentShiftValue, setCurrentShiftValue] = useState(8);
        const [debayerEnabled, setDebayerEnabled] = useState(false);
        const [viewMode, setViewMode] = useState(readInitialViewMode);
        const [runtimeMenu, setRuntimeMenu] = useState({ open: false, x: 0, y: 0, kind: 'background', runtimeId: null, nodeId: null, sources: [], processors: [], probes: [], sinks: [] });
        const [dialogState, setDialogState] = useState({ open: false, mode: null, runtimeId: null, runtimeName: '', runtimeIp: '', nodeType: '', nodeId: '' });
        const [editorError, setEditorError] = useState('');
        const [runtimeLogs, setRuntimeLogs] = useState({});
        const [runtimeLogPanels, setRuntimeLogPanels] = useState({});
        const [pendingEdgeSourceId, setPendingEdgeSourceId] = useState('');
        const [runtimeLayouts, setRuntimeLayouts] = useState(readInitialRuntimeLayouts);
        const [nodeLayouts, setNodeLayouts] = useState(readInitialNodeLayouts);
        const [remoteRuntimeStatuses, setRemoteRuntimeStatuses] = useState({});
        const [dragState, setDragState] = useState(null);
        const [runtimeIpDrafts, setRuntimeIpDrafts] = useState({});
        const [runtimeIpEditMode, setRuntimeIpEditMode] = useState({});
        const [editorZoom, setEditorZoom] = useState(1);
        const [editorPanX, setEditorPanX] = useState(0);
        const [editorPanY, setEditorPanY] = useState(0);
        const [editorPanning, setEditorPanning] = useState(false);
        const [editorPanOrigin, setEditorPanOrigin] = useState({ x: 0, y: 0, baseX: 0, baseY: 0 });
        const [viewZoom, setViewZoom] = useState(1);
        const [viewPanX, setViewPanX] = useState(0);
        const [viewPanY, setViewPanY] = useState(0);
        const [isPanning, setIsPanning] = useState(false);
        const [panOrigin, setPanOrigin] = useState({ x: 0, y: 0, baseX: 0, baseY: 0 });
        const [splitRatio, setSplitRatio] = useState(0.68);
        const [splitDragState, setSplitDragState] = useState(null);
        const [captureFps, setCaptureFps] = useState(0);
        const [renderFps, setRenderFps] = useState(0);
        const [viewerControlsOpen, setViewerControlsOpen] = useState(false);

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
        const currentShiftValueRef = useRef(currentShiftValue);
        const debayerEnabledRef = useRef(debayerEnabled);
        const suppressNextNodeSelectRef = useRef(false);

        const editorGraph = useMemo(
                () => buildEditorGraph(pipelineGraph, localDraftRuntimes, localIp, runtimeLayouts, nodeLayouts, status, remoteRuntimeStatuses),
                [pipelineGraph, localDraftRuntimes, localIp, runtimeLayouts, nodeLayouts, status, remoteRuntimeStatuses]
        );

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
        currentShiftValueRef.current = currentShiftValue;
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

        function clampNodePosInRuntime(runtimeId, nextPos) {
                const runtime = editorGraph.runtimes.find((item) => item.id === runtimeId);
                if (!runtime) {
                        return { x: Math.max(8, nextPos.x), y: Math.max(8, nextPos.y) };
                }
                const canvasWidth = Math.max(180, (runtime.rect?.w || 400) - 20);
                const canvasHeight = Math.max(120, (runtime.rect?.h || 260) - 60);
                return {
                        x: Math.max(8, Math.min(nextPos.x, canvasWidth - NODE_WIDTH - 8)),
                        y: Math.max(8, Math.min(nextPos.y, canvasHeight - NODE_HEIGHT - 8))
                };
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

        function canPlaceNode(runtimeId, nodeId, nextPos) {
                const runtime = editorGraph.runtimes.find((item) => item.id === runtimeId);
                if (!runtime) {
                        return false;
                }
                const canvasWidth = Math.max(180, (runtime.rect?.w || 400) - 20);
                const canvasHeight = Math.max(120, (runtime.rect?.h || 260) - 60);
                const clamped = {
                        x: Math.max(8, Math.min(nextPos.x, canvasWidth - NODE_WIDTH - 8)),
                        y: Math.max(8, Math.min(nextPos.y, canvasHeight - NODE_HEIGHT - 8))
                };
                const nextRect = { x: clamped.x, y: clamped.y, w: NODE_WIDTH, h: NODE_HEIGHT };
                const overlaps = runtime.nodes.some((node) => {
                        if (node.id === nodeId) {
                                return false;
                        }
                        const nodeRect = { x: node.x, y: node.y, w: NODE_WIDTH, h: NODE_HEIGHT };
                        return rectsOverlap(nextRect, nodeRect);
                });
                if (overlaps) {
                        return null;
                }
                return clamped;
        }

        function closeMenu() {
                setRuntimeMenu((menu) => ({ ...menu, open: false }));
        }

        function openMenu(event, kind, runtimeId = null, nodeId = null) {
                if (event.button === 2 && (editorPanGestureRef.current.active || editorPanGestureRef.current.moved)) {
                        event.preventDefault();
                        event.stopPropagation();
                        editorPanGestureRef.current = { active: false, moved: false, startX: 0, startY: 0, button: null };
                        return;
                }
                event.preventDefault();
                event.stopPropagation();
                editorPanGestureRef.current = { active: false, moved: false, startX: 0, startY: 0, button: null };
                setRuntimeMenu({
                        open: true,
                        x: event.clientX,
                        y: event.clientY,
                        kind,
                        runtimeId,
                        nodeId,
                        sources: nodeCatalog.sources.length ? nodeCatalog.sources : FALLBACK_NODE_TYPES.sources,
                        processors: nodeCatalog.processors.length ? nodeCatalog.processors : FALLBACK_NODE_TYPES.processors,
                        probes: nodeCatalog.probes.length ? nodeCatalog.probes : FALLBACK_NODE_TYPES.probes,
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
                        nextNodePos = clampNodePosInRuntime(runtime.id, {
                                x: graphX - runtimeRect.x - NODE_WIDTH * 0.5,
                                y: graphY - runtimeRect.y - RUNTIME_HEADER_HEIGHT - NODE_HEIGHT * 0.5
                        });
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
                if (event.target instanceof Element && event.target.closest('.node-edge-handle')) {
                        return;
                }
                event.stopPropagation();
                const runtime = editorGraph.runtimes.find((item) => item.id === runtimeId);
                const node = runtime?.nodes?.find((item) => item.id === nodeId);
                if (!node) {
                        return;
                }
                setDragState({ kind: 'node', runtimeId, nodeId, startX: event.clientX, startY: event.clientY, startPos: { x: node.x, y: node.y } });
        }

        function beginEdgeConnection(nodeId) {
                setPendingEdgeSourceId(nodeId);
                setEditorError('select a target node to create the edge');
        }

        async function connectNodes(sourceNodeId, targetNodeId) {
                if (!sourceNodeId || !targetNodeId || sourceNodeId === targetNodeId) {
                        setPendingEdgeSourceId('');
                        return;
                }
                const edgeText = `${sourceNodeId}.image -> ${targetNodeId}.image`;
                setPendingEdgeSourceId('');

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

        function renameRuntime(runtimeId) {
                const currentRuntime = editorGraph.runtimes.find((runtime) => runtime.id === runtimeId);
                if (!currentRuntime || runtimeId === LOCAL_RUNTIME_ID) return;
                const nextName = window.prompt('Runtime name', currentRuntime.name || runtimeId);
                if (!nextName) return;
                setLocalDraftRuntimes((current) => current.map((runtime) => (runtime.id === runtimeId ? { ...runtime, name: nextName } : runtime)));
                if (runtimeId === selectedRuntimeId) setSelectedRuntimeName(nextName);
        }

        function renameNode(nodeId) {
                const nextName = window.prompt('Node name', nodeId);
                if (!nextName) return;
                setLocalDraftRuntimes((current) => current.map((runtime) => ({
                        ...runtime,
                        nodes: runtime.nodes.map((node) => (node.id === nodeId ? { ...node, name: nextName } : node))
                })));
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
                const activeShiftValue = currentShiftValueRef.current;
                const activeDebayerEnabled = debayerEnabledRef.current;
                const cacheKey = `${meta.sequence}:${meta.formatId}:${meta.bitsPerPixel}:${meta.stride}:${activeShiftValue}:${activeDebayerEnabled ? 1 : 0}`;
                if (renderCacheRef.current.key === cacheKey && renderCacheRef.current.width === meta.width && renderCacheRef.current.height === meta.height && renderCacheRef.current.rgba) {
                        return renderRgbaToCanvas(meta.width, meta.height, renderCacheRef.current.rgba, resetView);
                }

                const rgba = renderPacketToRgba(meta, bytes, activeShiftValue, activeDebayerEnabled);

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
                        const discovery = buildEditorGraph(graph, localDraftRuntimes, localIp, runtimeLayouts, nodeLayouts, status);
                        setGraphStatusText(`discovered ${discovery.nodeIds.length} node(s) on ${discovery.runtimes.length} runtime(s)`);
                        const storedSelectedNodeId = readInitialSelectedNodeId();
                        if (!discovery.nodeIds.includes(selectedNodeId) && discovery.nodeIds.length > 0) {
                                const preferredNodeId = storedSelectedNodeId && discovery.nodeIds.includes(storedSelectedNodeId) ? storedSelectedNodeId : discovery.nodeIds[0];
                                setSelectedNodeId(preferredNodeId);
                        } else if (!discovery.nodeIds.includes(selectedNodeId) && discovery.nodeIds.length === 0) {
                                setSelectedNodeId('');
                        }
                        if (!discovery.runtimes.some((runtime) => runtime.id === selectedRuntimeId) && discovery.runtimes.length > 0) {
                                setSelectedRuntimeId(discovery.runtimes[0].id);
                                setSelectedRuntimeName(discovery.runtimes[0].name);
                        }
                } catch (_) {
                        setGraphStatusText('runtime discovery failed');
                }
        }

        async function fetchNodeParameters(nodeId) {
                if (!nodeId) return;
                if (!liveNodeIds.has(nodeId)) {
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
                        setSelectedNodeMeta((pipelineGraph.nodes || []).find((node) => node.id === nodeId) || null);
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
                                committedParameterValuesRef.current.set(updateKey, nextValue);
                                if (hasSideEffects) {
                                        await fetchNodeParameters(nodeId);
                                }
                        } catch (_) {
                                setEditorError(`failed to update ${name}`);
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

        function beginRuntimeIpEdit(runtime) {
                const value = runtime?.ip || runtimeDisplayLabel(runtime, localIp);
                setRuntimeIpDrafts((current) => ({ ...current, [runtime.id]: value }));
                setRuntimeIpEditMode((current) => ({ ...current, [runtime.id]: true }));
        }

        async function commitRuntimeIpEdit(runtime, commit) {
                if (!runtime) {
                        return;
                }
                const runtimeId = runtime.id;
                const draft = String(runtimeIpDrafts[runtimeId] ?? runtime.ip ?? '').trim();
                setRuntimeIpEditMode((current) => ({ ...current, [runtimeId]: false }));
                if (!commit || !draft || runtimeId === LOCAL_RUNTIME_ID) {
                        return;
                }

                const payload = buildGraphPayload(pipelineGraph, localDraftRuntimes);
                payload.nodes = (payload.nodes || []).map((node) => {
                        const nodeRuntimeId = runtimeIdFromNode(node, localIp);
                        if (nodeRuntimeId !== runtimeId) {
                                return node;
                        }
                        return {
                                ...node,
                                parameters: {
                                        ...(node.parameters || {}),
                                        runtimeTargetIp: draft
                                }
                        };
                });

                try {
                        const response = await fetch('/api/pipeline', {
                                method: 'PUT',
                                headers: { 'Content-Type': 'application/json' },
                                body: JSON.stringify(payload)
                        });
                        if (!response.ok) {
                                setEditorError('failed to update runtime ip');
                                return;
                        }
                        await fetchPipeline();
                } catch (_) {
                        setEditorError('failed to update runtime ip');
                }
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
        }, [currentShiftValue, debayerEnabled]);

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
                const canvas = frameCanvasRef.current;
                const viewport = frameViewportRef.current;
                if (!canvas || !viewport) return;
                const baseScale = Math.min((viewport.clientWidth || 1) / (canvas.width || 1), (viewport.clientHeight || 1) / (canvas.height || 1));
                const scale = baseScale * viewZoom;
                const scaledWidth = (canvas.width || 1) * scale;
                const scaledHeight = (canvas.height || 1) * scale;
                const baseX = ((viewport.clientWidth || 1) - scaledWidth) / 2;
                const baseY = ((viewport.clientHeight || 1) - scaledHeight) / 2;
                canvas.style.transform = `translate(${baseX + viewPanX}px, ${baseY + viewPanY}px) scale(${scale})`;
        }, [viewZoom, viewPanX, viewPanY, frameMeta]);

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
                const moveHandler = (event) => {
                        if (!event.buttons || !isPanning) return;
                        const gesture = viewerPanGestureRef.current;
                        if (gesture.active && !gesture.moved) {
                                const dx = event.clientX - gesture.startX;
                                const dy = event.clientY - gesture.startY;
                                if ((dx * dx + dy * dy) >= 9) {
                                        gesture.moved = true;
                                }
                        }
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
                        if (!event.buttons || !editorPanning) return;
                        const gesture = editorPanGestureRef.current;
                        if (gesture.active && !gesture.moved) {
                                const dx = event.clientX - gesture.startX;
                                const dy = event.clientY - gesture.startY;
                                if ((dx * dx + dy * dy) >= 9) {
                                        gesture.moved = true;
                                }
                        }
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
                                        x: dragState.startPos.x + dx,
                                        y: dragState.startPos.y + dy
                                };
                                const accepted = canPlaceNode(dragState.runtimeId, dragState.nodeId, nextPos);
                                if (!accepted) {
                                        return;
                                }
                                setNodeLayouts((current) => ({ ...current, [dragState.nodeId]: accepted }));
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
                return {
                        id: edge.id,
                        fromNode: edge.fromNode,
                        fromPort: edge.fromPort,
                        toNode: edge.toNode,
                        toPort: edge.toPort,
                        from: { x: fromRuntime.rect.x + fromNode.x, y: fromRuntime.rect.y + RUNTIME_HEADER_HEIGHT + fromNode.y },
                        to: { x: toRuntime.rect.x + toNode.x, y: toRuntime.rect.y + RUNTIME_HEADER_HEIGHT + toNode.y }
                };
        }).filter(Boolean);

        return (
                <div className="app-shell" onClick={closeMenu}>
                        <GlobalHeader
                                runtimeStatusText={runtimeStatusText}
                                versionParts={versionParts}
                                graphStatusText={graphStatusText}
                                runtimeRunning={runtimeRunning}
                                onToggleRuntime={() => void setPipelineStopped(runtimeRunning)}
                                viewMode={viewMode}
                                onSetViewMode={setViewMode}
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
                                                if (target && target.closest('.runtime-log-console,.runtime-log-filter-panel,.runtime-log-scroll-area,.runtime-log-search-field')) {
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
                                        onResetView={() => { setEditorZoom(1); setEditorPanX(0); setEditorPanY(0); }}
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
                                        runtimeIpEditMode={runtimeIpEditMode}
                                        runtimeIpDrafts={runtimeIpDrafts}
                                        beginRuntimeIpEdit={beginRuntimeIpEdit}
                                        setRuntimeIpDrafts={setRuntimeIpDrafts}
                                        renameRuntime={renameRuntime}
                                        renameNode={renameNode}
                                        selectedNodeId={selectedNodeId}
                                        suppressNextNodeSelectRef={suppressNextNodeSelectRef}
                                        pendingEdgeSourceId={pendingEdgeSourceId}
                                        connectNodes={connectNodes}
                                        setSelectedNodeId={setSelectedNodeId}
                                        setPendingEdgeSourceId={setPendingEdgeSourceId}
                                        openMenu={openMenu}
                                        setSelectedRuntimeId={setSelectedRuntimeId}
                                        edgeCurvePath={edgeCurvePath}
                                        startNodeDrag={startNodeDrag}
                                        startRuntimeDrag={startRuntimeDrag}
                                        startRuntimeResize={startRuntimeResize}
                                        onStartRuntime={onStartRuntime}
                                        commitRuntimeIpEdit={commitRuntimeIpEdit}
                                        runtimeDisplayLabel={runtimeDisplayLabel}
                                        runtimeLogs={runtimeLogs}
                                        runtimeLogPanels={runtimeLogPanels}
                                        onToggleRuntimeLogPanel={onToggleRuntimeLogPanel}
                                        onClearRuntimeLogs={onClearRuntimeLogs}
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
                                        viewerControlsOpen={viewerControlsOpen}
                                        setViewerControlsOpen={setViewerControlsOpen}
                                        hasFrame={hasFrame}
                                        currentShiftValue={currentShiftValue}
                                        setCurrentShiftValue={setCurrentShiftValue}
                                        debayerEnabled={debayerEnabled}
                                        setDebayerEnabled={setDebayerEnabled}
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
                                        editorError={editorError}
                                />
                        </main>

                        {dialogState.open && dialogState.mode === 'runtime' ? (
                                <div className="dialog-backdrop" onClick={() => setDialogState({ open: false, mode: null, runtimeId: null, runtimeName: '', runtimeIp: '', nodeType: '', nodeId: '' })}>
                                        <div className="dialog" onClick={(event) => event.stopPropagation()}>
                                                <h3>New Runtime</h3>
                                                <label>
                                                        Runtime Name
                                                        <input
                                                                type="text"
                                                                value={dialogState.runtimeName}
                                                                onChange={(event) => setDialogState((current) => ({ ...current, runtimeName: event.target.value }))}
                                                        />
                                                </label>
                                                <label>
                                                        Runtime IP
                                                        <input
                                                                type="text"
                                                                value={dialogState.runtimeIp}
                                                                onChange={(event) => setDialogState((current) => ({ ...current, runtimeIp: event.target.value }))}
                                                        />
                                                </label>
                                                <div className="dialog-actions">
                                                        <UiButton className="secondary" variant="secondary" type="button" onClick={() => setDialogState({ open: false, mode: null, runtimeId: null, runtimeName: '', runtimeIp: '', nodeType: '', nodeId: '' })}>Cancel</UiButton>
                                                        <UiButton type="button" onClick={addRuntime}>Create</UiButton>
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
                                localRuntimeId={LOCAL_RUNTIME_ID}
                        />
                </div>
        );
}