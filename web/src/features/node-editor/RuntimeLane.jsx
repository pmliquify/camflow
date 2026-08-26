import React, { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import RuntimeWindowIcon from './RuntimeWindowIcon.jsx';
import NodeCard from './NodeCard.jsx';
import EdgeLayer from './EdgeLayer.jsx';
import RuntimeLogConsole from './RuntimeLogConsole.jsx';
import MediaGraphView from './MediaGraphView.jsx';
import DeviceTreeView from './DeviceTreeView.jsx';
import ModuleDebugView from './ModuleDebugView.jsx';
import logIcon from '../../assets/images/icon-log.svg';
import mediaGraphIcon from '../../assets/images/icon-media-graph.svg';
import moduleDebugIcon from '../../assets/images/icon-debug-bug.svg';
import reloadIcon from '../../assets/images/icon-reload.svg';
import closeIcon from '../../assets/images/icon-close.svg';
import Button from '../../components/Button.jsx';
import Input from '../../components/Input.jsx';
import InlineNameEditor from '../../components/InlineNameEditor.jsx';
import ResetViewButton from '../../components/ResetViewButton.jsx';
import { getDeviceTree, getMediaDevices, getMediaGraph, getModuleDebugList, setModuleDebugLevel } from '../../services/runtimeApi.js';

const RUNTIME_LOG_PREFS_STORAGE_PREFIX = 'camflow:runtime-log-prefs:';
const RUNTIME_LOG_DEFAULT_FONT_STORAGE_KEY = 'camflow:runtime-log-default-font-size';
const RUNTIME_MEDIA_PREFS_STORAGE_PREFIX = 'camflow:runtime-media-prefs:';
const RUNTIME_DEVICE_TREE_PREFS_STORAGE_PREFIX = 'camflow:runtime-devicetree-prefs:';
const RUNTIME_MODULE_DEBUG_PREFS_STORAGE_PREFIX = 'camflow:runtime-moduledebug-prefs:';
const LOG_FONT_MIN = 5;
const LOG_FONT_MAX = 15;
const LOG_FONT_DEFAULT = 10;
const LOG_CONSOLE_MIN_HEIGHT = 112;
const LOG_CONSOLE_MAX_HEIGHT = 320;
const LOG_CONSOLE_DEFAULT_HEIGHT = 154;
const RUNTIME_MIN_ZOOM = 0.01;
const RUNTIME_MAX_ZOOM = 2.25;
const RUNTIME_PAN_DRAG_THRESHOLD = 8;
const DEFAULT_VISIBLE_LOG_SOURCES = { application: true, runtime: true, node: true, api: true, kernel: true };

function clampLogFontSize(value) {
        const parsed = Number(value);
        if (!Number.isFinite(parsed)) {
                return LOG_FONT_DEFAULT;
        }
        return Math.max(LOG_FONT_MIN, Math.min(LOG_FONT_MAX, Math.round(parsed)));
}

function clampLogConsoleHeight(value) {
        const parsed = Number(value);
        if (!Number.isFinite(parsed)) {
                return LOG_CONSOLE_DEFAULT_HEIGHT;
        }
        return Math.max(LOG_CONSOLE_MIN_HEIGHT, Math.min(LOG_CONSOLE_MAX_HEIGHT, Math.round(parsed)));
}

function loadRuntimeLogPrefs(runtimeId) {
        if (typeof window === 'undefined') {
                return null;
        }
        const key = `${RUNTIME_LOG_PREFS_STORAGE_PREFIX}${runtimeId}`;
        try {
                const raw = window.localStorage.getItem(key);
                if (!raw) {
                        return null;
                }
                const parsed = JSON.parse(raw);
                return {
                        filterOpen: Boolean(parsed?.filterOpen),
                        showDebugDetails: Boolean(parsed?.showDebugDetails),
                        visibleSources: {
                                application: parsed?.visibleSources?.application !== false,
                                runtime: parsed?.visibleSources?.runtime !== false,
                                node: parsed?.visibleSources?.node !== false,
                                api: parsed?.visibleSources?.api !== false,
                                kernel: parsed?.visibleSources?.kernel !== false
                        },
                        kernelRegexText: String(parsed?.kernelRegexText || ''),
                        logFontSize: clampLogFontSize(parsed?.logFontSize),
                        logConsoleHeight: clampLogConsoleHeight(parsed?.logConsoleHeight)
                };
        } catch (_) {
                return null;
        }
}

function loadGlobalDefaultLogFontSize() {
        if (typeof window === 'undefined') {
                return LOG_FONT_DEFAULT;
        }
        try {
                return clampLogFontSize(window.localStorage.getItem(RUNTIME_LOG_DEFAULT_FONT_STORAGE_KEY));
        } catch (_) {
                return LOG_FONT_DEFAULT;
        }
}

function loadRuntimeMediaPrefs(runtimeId) {
        if (typeof window === 'undefined') {
                return null;
        }
        try {
                const raw = window.localStorage.getItem(`${RUNTIME_MEDIA_PREFS_STORAGE_PREFIX}${runtimeId}`);
                if (!raw) {
                        return null;
                }
                const parsed = JSON.parse(raw);
                return {
                        device: typeof parsed?.device === 'string' ? parsed.device : '',
                        connectedOnly: Boolean(parsed?.connectedOnly)
                };
        } catch (_) {
                return null;
        }
}

function loadRuntimeDeviceTreeQuery(runtimeId) {
        if (typeof window === 'undefined') {
                return '';
        }
        try {
                return String(window.localStorage.getItem(`${RUNTIME_DEVICE_TREE_PREFS_STORAGE_PREFIX}${runtimeId}`) || '');
        } catch (_) {
                return '';
        }
}

function loadRuntimeModuleDebugQuery(runtimeId) {
        if (typeof window === 'undefined') {
                return '';
        }
        try {
                return String(window.localStorage.getItem(`${RUNTIME_MODULE_DEBUG_PREFS_STORAGE_PREFIX}${runtimeId}`) || '');
        } catch (_) {
                return '';
        }
}

export default function RuntimeLane({
        viewMode,
        runtime,
        logEntries,
        logOpen,
        onToggleLogOpen,
        selectedNodeId,
        onSelectNode,
        onLaneContextMenu,
        onEdgePath,
        onEdgeContextMenu,
        onNodeDragStart,
        onNodeContextMenu,
        onNodePortContextMenu,
        onNodePortMouseDown,
        onRuntimeDragStart,
        onRuntimeResizeStart,
        onStartRuntime,
        onClearRuntimeLogs,
        logFilterOpenRequest,
        deviceTreeOpenRequest,
        onDeviceTreeContextMenu,
        filterCloseRequest = 0,
        runtimeBaseUrl = '',
        selectedMediaElement,
        onSelectMediaElement,
        runtimeViewports = {},
        onRuntimeViewportChange
}) {
        const initialLogPrefs = useMemo(() => loadRuntimeLogPrefs(runtime.id), [runtime.id]);
        const initialMediaPrefs = useMemo(() => loadRuntimeMediaPrefs(runtime.id), [runtime.id]);
        const [filterOpen, setFilterOpen] = useState(() => initialLogPrefs?.filterOpen || false);
        const [visibleSources, setVisibleSources] = useState(() => initialLogPrefs?.visibleSources || DEFAULT_VISIBLE_LOG_SOURCES);
        const [kernelRegexText, setKernelRegexText] = useState(() => initialLogPrefs?.kernelRegexText || '');
        const [showDebugDetails, setShowDebugDetails] = useState(() => initialLogPrefs?.showDebugDetails || false);
        const [logFontSize, setLogFontSize] = useState(() => initialLogPrefs?.logFontSize || loadGlobalDefaultLogFontSize());
        const [logConsoleHeight, setLogConsoleHeight] = useState(() => initialLogPrefs?.logConsoleHeight || LOG_CONSOLE_DEFAULT_HEIGHT);
        const [mediaMode, setMediaMode] = useState(false);
        const [mediaDevices, setMediaDevices] = useState([]);
        const [mediaDevice, setMediaDevice] = useState(() => initialMediaPrefs?.device || '');
        const [mediaGraph, setMediaGraph] = useState(null);
        const [mediaLoading, setMediaLoading] = useState(false);
        const [mediaError, setMediaError] = useState('');
        const [mediaConnectedOnly, setMediaConnectedOnly] = useState(() => initialMediaPrefs?.connectedOnly || false);
        const [deviceTreeMode, setDeviceTreeMode] = useState(false);
        const [deviceTree, setDeviceTree] = useState(null);
        const [deviceTreeLoading, setDeviceTreeLoading] = useState(false);
        const [deviceTreeError, setDeviceTreeError] = useState('');
        const [deviceTreeQuery, setDeviceTreeQuery] = useState(() => loadRuntimeDeviceTreeQuery(runtime.id));
        const [deviceTreeStatus, setDeviceTreeStatus] = useState({ text: '', invalid: false });
        const [deviceTreeFocusSequence, setDeviceTreeFocusSequence] = useState(0);
        const deviceTreeSearchRef = useRef(null);
        const [moduleDebugMode, setModuleDebugMode] = useState(false);
        const [moduleDebugList, setModuleDebugList] = useState([]);
        const [moduleDebugLoading, setModuleDebugLoading] = useState(false);
        const [moduleDebugError, setModuleDebugError] = useState('');
        const [moduleDebugDrafts, setModuleDebugDrafts] = useState({});
        const [moduleDebugApplying, setModuleDebugApplying] = useState({});
        const [moduleDebugQuery, setModuleDebugQuery] = useState(() => loadRuntimeModuleDebugQuery(runtime.id));
        const [moduleDebugStatus, setModuleDebugStatus] = useState({ text: '', invalid: false });
        const moduleDebugSearchRef = useRef(null);
        const runtimePanGestureRef = useRef({ moved: false, button: null });
        const runtimePanCleanupRef = useRef(null);
        const runtimeCanvasRef = useRef(null);
        const hasAutoCenteredRef = useRef({ node: false, media: false });

        const runtimeViewportKind = mediaMode ? 'media' : 'node';
        const runtimeViewport = runtimeViewports[runtimeViewportKind] || { zoom: 1, panX: 0, panY: 0 };
        const hasStoredRuntimeViewport = Boolean(runtimeViewports[runtimeViewportKind]);
        const runtimeZoom = runtimeViewport.zoom || 1;
        const runtimePanX = runtimeViewport.panX || 0;
        const runtimePanY = runtimeViewport.panY || 0;
        const runtimeNodeIdsSignature = runtime.nodes.map((node) => node.id).join('|');
        const mediaGraphSignature = (mediaGraph?.entities || []).map((entity) => entity.id).join('|');

        const updateRuntimeViewport = (update) => {
                onRuntimeViewportChange?.(runtime.id, runtimeViewportKind, update);
        };

        const runtimeViewportElement = (canvas) => {
                if (!mediaMode) {
                        return canvas;
                }
                return canvas.querySelector('.media-graph-scroll') || canvas;
        };

        const resetOrFitRuntimeViewport = () => {
                const canvas = runtimeCanvasRef.current;
                if (!canvas || deviceTreeMode || moduleDebugMode) {
                        return false;
                }

                const viewport = runtimeViewportElement(canvas);
                if (viewport.clientWidth === 0 || viewport.clientHeight === 0) {
                        return false;
                }

                let minX = 0;
                let minY = 0;
                let maxX = 0;
                let maxY = 0;
                if (mediaMode) {
                        const graphCanvas = canvas.querySelector('.media-graph-canvas');
                        if (!(graphCanvas instanceof HTMLElement)) {
                                return false;
                        }
                        maxX = Number.parseFloat(graphCanvas.style.width) || graphCanvas.offsetWidth;
                        maxY = Number.parseFloat(graphCanvas.style.height) || graphCanvas.offsetHeight;
                } else {
                        const nodes = [...canvas.querySelectorAll('.node-card')];
                        if (nodes.length === 0) {
                                return false;
                        }
                        minX = Math.min(...nodes.map((node) => node.offsetLeft));
                        minY = Math.min(...nodes.map((node) => node.offsetTop));
                        maxX = Math.max(...nodes.map((node) => node.offsetLeft + node.offsetWidth));
                        maxY = Math.max(...nodes.map((node) => node.offsetTop + node.offsetHeight));
                        const edgeLayer = canvas.querySelector('.edge-layer');
                        if (edgeLayer instanceof SVGGraphicsElement) {
                                const edgeBounds = edgeLayer.getBBox();
                                if (edgeBounds.width > 0 || edgeBounds.height > 0) {
                                        minX = Math.min(minX, edgeBounds.x);
                                        minY = Math.min(minY, edgeBounds.y);
                                        maxX = Math.max(maxX, edgeBounds.x + edgeBounds.width);
                                        maxY = Math.max(maxY, edgeBounds.y + edgeBounds.height);
                                }
                        }
                }

                const contentWidth = Math.max(1, maxX - minX);
                const contentHeight = Math.max(1, maxY - minY);
                viewport.scrollLeft = 0;
                viewport.scrollTop = 0;

                const padding = 16;
                const availableWidth = Math.max(1, viewport.clientWidth - padding * 2);
                const availableHeight = Math.max(1, viewport.clientHeight - padding * 2);
                const zoom = Math.max(0.01, Math.min(1, availableWidth / contentWidth, availableHeight / contentHeight));
                updateRuntimeViewport({
                        zoom,
                        panX: (viewport.clientWidth - contentWidth * zoom) / 2 - minX * zoom,
                        panY: (viewport.clientHeight - contentHeight * zoom) / 2 - minY * zoom
                });
                return true;
        };

        useEffect(() => {
                const hasContent = mediaMode ? (mediaGraph?.entities || []).length > 0 : runtime.nodes.length > 0;
                if (hasStoredRuntimeViewport || hasAutoCenteredRef.current[runtimeViewportKind] || !hasContent) {
                        return undefined;
                }
                const timerId = window.setTimeout(() => {
                        if (resetOrFitRuntimeViewport()) {
                                hasAutoCenteredRef.current[runtimeViewportKind] = true;
                        }
                }, 0);
                return () => window.clearTimeout(timerId);
                // Fit each view once after its first content render.
                // eslint-disable-next-line react-hooks/exhaustive-deps
        }, [hasStoredRuntimeViewport, mediaGraphSignature, mediaMode, runtimeNodeIdsSignature, viewMode]);

        const onRuntimeWheelCapture = (event) => {
                if (deviceTreeMode || moduleDebugMode) {
                        return;
                }
                event.preventDefault();
                event.stopPropagation();

                if (event.shiftKey || event.altKey) {
                        updateRuntimeViewport((current) => ({
                                ...current,
                                panX: current.panX - (event.shiftKey ? event.deltaY : 0),
                                panY: current.panY - (event.altKey ? event.deltaY : 0)
                        }));
                        return;
                }

                const canvas = event.currentTarget;
                const viewport = runtimeViewportElement(canvas);
                const rect = viewport.getBoundingClientRect();
                const outerScaleX = rect.width > 0 ? viewport.offsetWidth / rect.width : 1;
                const outerScaleY = rect.height > 0 ? viewport.offsetHeight / rect.height : outerScaleX;
                const cursorX = (event.clientX - rect.left) * outerScaleX + (viewport.scrollLeft || 0);
                const cursorY = (event.clientY - rect.top) * outerScaleY + (viewport.scrollTop || 0);
                const factor = Math.exp(-event.deltaY * 0.0016);
                updateRuntimeViewport((current) => {
                        const currentZoom = current.zoom || 1;
                        const currentPanX = current.panX || 0;
                        const currentPanY = current.panY || 0;
                        const nextZoom = Math.max(RUNTIME_MIN_ZOOM, Math.min(RUNTIME_MAX_ZOOM, currentZoom * factor));
                        if (nextZoom === currentZoom) {
                                return current;
                        }
                        const graphX = (cursorX - currentPanX) / currentZoom;
                        const graphY = (cursorY - currentPanY) / currentZoom;
                        return {
                                zoom: nextZoom,
                                panX: cursorX - graphX * nextZoom,
                                panY: cursorY - graphY * nextZoom
                        };
                });
        };

        const onRuntimePanMouseDownCapture = (event) => {
                if (deviceTreeMode || moduleDebugMode) {
                        return;
                }
                if ((event.button !== 1 && event.button !== 2) || (event.target instanceof Element && event.target.closest('button,input,select,textarea'))) {
                        return;
                }
                runtimePanCleanupRef.current?.();
                event.preventDefault();
                event.stopPropagation();
                const canvas = event.currentTarget;
                const rect = canvas.getBoundingClientRect();
                const outerScaleX = rect.width > 0 ? canvas.offsetWidth / rect.width : 1;
                const outerScaleY = rect.height > 0 ? canvas.offsetHeight / rect.height : outerScaleX;
                const startX = event.clientX;
                const startY = event.clientY;
                const startPanX = runtimePanX;
                const startPanY = runtimePanY;
                runtimePanGestureRef.current = { moved: false, button: event.button };

                const moveHandler = (moveEvent) => {
                        const buttonMask = event.button === 1 ? 4 : 2;
                        if ((moveEvent.buttons & buttonMask) === 0) {
                                stopHandler();
                                return;
                        }
                        const deltaX = moveEvent.clientX - startX;
                        const deltaY = moveEvent.clientY - startY;
                        if (!runtimePanGestureRef.current.moved && deltaX * deltaX + deltaY * deltaY >= RUNTIME_PAN_DRAG_THRESHOLD * RUNTIME_PAN_DRAG_THRESHOLD) {
                                runtimePanGestureRef.current.moved = true;
                        }
                        if (!runtimePanGestureRef.current.moved) {
                                return;
                        }
                        updateRuntimeViewport((current) => ({
                                ...current,
                                panX: startPanX + deltaX * outerScaleX,
                                panY: startPanY + deltaY * outerScaleY
                        }));
                };
                function stopHandler() {
                        runtimePanGestureRef.current = { moved: false, button: null };
                        runtimePanCleanupRef.current = null;
                        window.removeEventListener('mousemove', moveHandler);
                        window.removeEventListener('mouseup', stopHandler);
                        window.removeEventListener('blur', stopHandler);
                }
                window.addEventListener('mousemove', moveHandler);
                window.addEventListener('mouseup', stopHandler);
                window.addEventListener('blur', stopHandler);
                runtimePanCleanupRef.current = stopHandler;
        };

        const onRuntimeContextMenuMouseUpCapture = (event) => {
                const gesture = { ...runtimePanGestureRef.current };
                if (event.button === gesture.button) {
                        runtimePanCleanupRef.current?.();
                }
                if (event.button !== 2 || gesture.button !== 2 || gesture.moved) {
                        return;
                }
                const target = event.target instanceof Element ? event.target : null;
                if (!target || target.closest('.edge-hit-path,button,input,select,textarea')) {
                        return;
                }
                if (target.closest('.node-card')) {
                        return;
                }
                onLaneContextMenu(event, runtime.id);
        };

        const filterActive = useMemo(() => {
                return Object.values(visibleSources).some((value) => !value) || Boolean(kernelRegexText.trim());
        }, [kernelRegexText, visibleSources]);

        useEffect(() => {
                if (logFilterOpenRequest?.runtimeId === runtime.id && logFilterOpenRequest.sequence > 0) {
                        setFilterOpen(true);
                }
        }, [logFilterOpenRequest, runtime.id]);

        useEffect(() => {
                if (filterCloseRequest > 0) {
                        setFilterOpen(false);
                }
        }, [filterCloseRequest]);

        useEffect(() => {
                if (typeof window === 'undefined') {
                        return;
                }
                const key = `${RUNTIME_LOG_PREFS_STORAGE_PREFIX}${runtime.id}`;
                const payload = {
                        filterOpen,
                        showDebugDetails,
                        visibleSources,
                        kernelRegexText,
                        logFontSize: clampLogFontSize(logFontSize),
                        logConsoleHeight: clampLogConsoleHeight(logConsoleHeight)
                };
                try {
                        window.localStorage.setItem(key, JSON.stringify(payload));
                        window.localStorage.setItem(RUNTIME_LOG_DEFAULT_FONT_STORAGE_KEY, String(payload.logFontSize));
                } catch (_) {
                        // Ignore persistence failures (quota/private mode).
                }
        }, [filterOpen, kernelRegexText, logConsoleHeight, logFontSize, runtime.id, showDebugDetails, visibleSources]);

        useEffect(() => {
                if (typeof window === 'undefined') {
                        return;
                }
                try {
                        window.localStorage.setItem(`${RUNTIME_MEDIA_PREFS_STORAGE_PREFIX}${runtime.id}`, JSON.stringify({
                                device: mediaDevice,
                                connectedOnly: mediaConnectedOnly
                        }));
                } catch (_) {
                        // Ignore persistence failures (quota/private mode).
                }
        }, [mediaConnectedOnly, mediaDevice, runtime.id]);

        useEffect(() => {
                if (typeof window === 'undefined') {
                        return;
                }
                try {
                        window.localStorage.setItem(`${RUNTIME_DEVICE_TREE_PREFS_STORAGE_PREFIX}${runtime.id}`, deviceTreeQuery);
                } catch (_) {
                        // Ignore persistence failures (quota/private mode).
                }
        }, [deviceTreeQuery, runtime.id]);

        useEffect(() => {
                if (typeof window === 'undefined') {
                        return;
                }
                try {
                        window.localStorage.setItem(`${RUNTIME_MODULE_DEBUG_PREFS_STORAGE_PREFIX}${runtime.id}`, moduleDebugQuery);
                } catch (_) {
                        // Ignore persistence failures (quota/private mode).
                }
        }, [moduleDebugQuery, runtime.id]);

        async function loadDeviceTree() {
                setDeviceTreeLoading(true);
                setDeviceTreeError('');
                try {
                        setDeviceTree(await getDeviceTree(runtimeBaseUrl));
                } catch (error) {
                        setDeviceTree(null);
                        setDeviceTreeError(error instanceof Error ? error.message : 'device tree unavailable');
                } finally {
                        setDeviceTreeLoading(false);
                }
        }

        async function loadMediaGraph(requestedDevice = mediaDevice) {
                setMediaLoading(true);
                setMediaError('');
                onSelectMediaElement?.(null);
                try {
                        const payload = await getMediaDevices(runtimeBaseUrl);
                        const devices = Array.isArray(payload?.devices) ? payload.devices : [];
                        setMediaDevices(devices);
                        const nextDevice = devices.includes(requestedDevice) ? requestedDevice : (devices[0] || '');
                        setMediaDevice(nextDevice);
                        if (!nextDevice) {
                                setMediaGraph(null);
                                setMediaError('no /dev/mediaX devices');
                                return;
                        }
                        setMediaGraph(await getMediaGraph(nextDevice, runtimeBaseUrl));
                } catch (error) {
                        setMediaGraph(null);
                        setMediaError(error instanceof Error ? error.message : 'media graph unavailable');
                } finally {
                        setMediaLoading(false);
                }
        }

        useEffect(() => {
                if (mediaMode) void loadMediaGraph();
                // Reload only when the mode is activated or the target runtime changes.
                // eslint-disable-next-line react-hooks/exhaustive-deps
        }, [mediaMode, runtimeBaseUrl]);

        useEffect(() => {
                if (deviceTreeMode) void loadDeviceTree();
                // Reload only when the mode is activated or the target runtime changes.
                // eslint-disable-next-line react-hooks/exhaustive-deps
        }, [deviceTreeMode, runtimeBaseUrl]);

        async function loadModuleDebug() {
                setModuleDebugLoading(true);
                setModuleDebugError('');
                try {
                        const payload = await getModuleDebugList(runtimeBaseUrl);
                        const modules = Array.isArray(payload?.modules) ? payload.modules : [];
                        setModuleDebugList(modules);
                        setModuleDebugDrafts(Object.fromEntries(modules.map((module) => [module.name, module.value])));
                } catch (error) {
                        setModuleDebugList([]);
                        setModuleDebugError(error instanceof Error ? error.message : 'module debug list unavailable');
                } finally {
                        setModuleDebugLoading(false);
                }
        }

        useEffect(() => {
                if (moduleDebugMode) void loadModuleDebug();
                // Reload only when the mode is activated or the target runtime changes.
                // eslint-disable-next-line react-hooks/exhaustive-deps
        }, [moduleDebugMode, runtimeBaseUrl]);

        const onModuleDebugDraftChange = useCallback((moduleName, value) => {
                setModuleDebugDrafts((current) => ({ ...current, [moduleName]: value }));
        }, []);

        const onModuleDebugCommit = useCallback(async (moduleName) => {
                const module = moduleDebugList.find((item) => item.name === moduleName);
                const draftValue = moduleDebugDrafts[moduleName];
                if (!module || draftValue === undefined || String(draftValue) === String(module.value)) {
                        return;
                }
                setModuleDebugApplying((current) => ({ ...current, [moduleName]: true }));
                try {
                        await setModuleDebugLevel(moduleName, draftValue, runtimeBaseUrl);
                        setModuleDebugList((current) => current.map((item) => (item.name === moduleName ? { ...item, value: String(draftValue) } : item)));
                        setModuleDebugError('');
                } catch (error) {
                        setModuleDebugDrafts((current) => ({ ...current, [moduleName]: module.value }));
                        setModuleDebugError(error instanceof Error ? error.message : 'failed to set debug level');
                } finally {
                        setModuleDebugApplying((current) => ({ ...current, [moduleName]: false }));
                }
        }, [moduleDebugDrafts, moduleDebugList, runtimeBaseUrl]);

        useEffect(() => {
                if (deviceTreeOpenRequest?.runtimeId !== runtime.id || deviceTreeOpenRequest.sequence <= 0) {
                        return;
                }
                setDeviceTreeMode(true);
                setMediaMode(false);
                setModuleDebugMode(false);
                onSelectMediaElement?.(null);
                setDeviceTreeFocusSequence(deviceTreeOpenRequest.sequence);
                // eslint-disable-next-line react-hooks/exhaustive-deps
        }, [deviceTreeOpenRequest, runtime.id]);

        useEffect(() => {
                if (!deviceTreeMode || deviceTreeFocusSequence === 0) {
                        return;
                }
                deviceTreeSearchRef.current?.focus();
                deviceTreeSearchRef.current?.select();
        }, [deviceTreeFocusSequence, deviceTreeMode]);

        const onDeviceTreeStatus = useCallback((text, invalid) => setDeviceTreeStatus({ text, invalid }), []);

        const onModuleDebugStatus = useCallback((text, invalid) => setModuleDebugStatus({ text, invalid }), []);

        return (
                <section
                        className="runtime-lane"
                        data-runtime-id={runtime.id}
                        style={{ left: runtime.rect.x, top: runtime.rect.y, width: runtime.rect.w, height: runtime.rect.h }}
                        onMouseDown={(event) => {
                                if (event.button !== 0) {
                                        return;
                                }
                                const bounds = event.currentTarget.getBoundingClientRect();
                                const nearRight = bounds.right - event.clientX <= 14;
                                const nearBottom = bounds.bottom - event.clientY <= 14;
                                if (nearRight && nearBottom) {
                                        onRuntimeResizeStart(event, runtime.id);
                                }
                        }}
                >
                        <header onMouseDown={(event) => onRuntimeDragStart(event, runtime.id)}>
                                <div className="runtime-header-left">
                                        <RuntimeWindowIcon />
                                        <InlineNameEditor
                                                value={runtime.displayName || runtime.ip}
                                                onCommit={runtime.onRename}
                                                className="runtime-name-text"
                                                inputClassName="inline-name-input runtime-name-editor"
                                                ariaLabel="runtime name"
                                        />
                                        <span className={`runtime-state state-${runtime.status}`}>{runtime.status}</span>
                                </div>
                                <div className="runtime-header-tools">
                                        {deviceTreeMode ? (
                                                <div className="parameter-search-field runtime-device-tree-search">
                                                        <Input
                                                                ref={deviceTreeSearchRef}
                                                                type="text"
                                                                placeholder="search device tree (regex)"
                                                                aria-label="search device tree"
                                                                value={deviceTreeQuery}
                                                                onMouseDown={(event) => event.stopPropagation()}
                                                                onChange={(event) => setDeviceTreeQuery(event.target.value)}
                                                                onKeyDown={(event) => {
                                                                        if (event.key === 'Escape') {
                                                                                event.stopPropagation();
                                                                                event.currentTarget.blur();
                                                                        }
                                                                }}
                                                        />
                                                        <Button
                                                                className={`parameter-search-clear${deviceTreeQuery ? '' : ' is-empty'}`}
                                                                type="button"
                                                                title="clear search"
                                                                icon={closeIcon}
                                                                iconOnly={true}
                                                                onMouseDown={(event) => event.stopPropagation()}
                                                                onClick={() => {
                                                                        setDeviceTreeQuery('');
                                                                        deviceTreeSearchRef.current?.focus();
                                                                }}
                                                        />
                                                </div>
                                        ) : null}
                                        {deviceTreeMode ? (
                                                <span className={`runtime-device-tree-status${deviceTreeStatus.invalid ? ' is-error' : ''}`}>{deviceTreeStatus.text}</span>
                                        ) : null}
                                        {deviceTreeMode ? (
                                                <Button
                                                        className="secondary runtime-device-tree-reload"
                                                        variant="secondary"
                                                        type="button"
                                                        title="reload device tree"
                                                        aria-label="reload device tree"
                                                        icon={reloadIcon}
                                                        iconOnly={true}
                                                        disabled={deviceTreeLoading}
                                                        onMouseDown={(event) => event.stopPropagation()}
                                                        onClick={() => void loadDeviceTree()}
                                                />
                                        ) : null}
                                        {moduleDebugMode ? (
                                                <div className="parameter-search-field runtime-module-debug-search">
                                                        <Input
                                                                ref={moduleDebugSearchRef}
                                                                type="text"
                                                                placeholder="search modules (regex)"
                                                                aria-label="search kernel modules"
                                                                value={moduleDebugQuery}
                                                                onMouseDown={(event) => event.stopPropagation()}
                                                                onChange={(event) => setModuleDebugQuery(event.target.value)}
                                                                onKeyDown={(event) => {
                                                                        if (event.key === 'Escape') {
                                                                                event.stopPropagation();
                                                                                event.currentTarget.blur();
                                                                        }
                                                                }}
                                                        />
                                                        <Button
                                                                className={`parameter-search-clear${moduleDebugQuery ? '' : ' is-empty'}`}
                                                                type="button"
                                                                title="clear search"
                                                                icon={closeIcon}
                                                                iconOnly={true}
                                                                onMouseDown={(event) => event.stopPropagation()}
                                                                onClick={() => {
                                                                        setModuleDebugQuery('');
                                                                        moduleDebugSearchRef.current?.focus();
                                                                }}
                                                        />
                                                </div>
                                        ) : null}
                                        {moduleDebugMode ? (
                                                <span className={`runtime-device-tree-status${moduleDebugStatus.invalid ? ' is-error' : ''}`}>{moduleDebugStatus.text}</span>
                                        ) : null}
                                        {moduleDebugMode ? (
                                                <Button
                                                        className="secondary runtime-module-debug-reload"
                                                        variant="secondary"
                                                        type="button"
                                                        title="reload kernel modules"
                                                        aria-label="reload kernel modules"
                                                        icon={reloadIcon}
                                                        iconOnly={true}
                                                        disabled={moduleDebugLoading}
                                                        onMouseDown={(event) => event.stopPropagation()}
                                                        onClick={() => void loadModuleDebug()}
                                                />
                                        ) : null}
                                        {mediaMode ? (
                                                <select
                                                        className="runtime-media-device"
                                                        value={mediaDevice}
                                                        aria-label="media device"
                                                        onMouseDown={(event) => event.stopPropagation()}
                                                        onChange={(event) => void loadMediaGraph(event.target.value)}
                                                >
                                                        {mediaDevices.map((device) => <option key={device} value={device}>{device}</option>)}
                                                </select>
                                        ) : null}
                                        {mediaMode ? (
                                                <Button
                                                        className={`secondary runtime-media-connected-toggle${mediaConnectedOnly ? ' active' : ''}`}
                                                        variant="secondary"
                                                        compact={true}
                                                        type="button"
                                                        title="show connected links only"
                                                        aria-label="show connected links only"
                                                        onMouseDown={(event) => event.stopPropagation()}
                                                        onClick={() => {
                                                                setMediaConnectedOnly((current) => !current);
                                                                onSelectMediaElement?.(null);
                                                        }}
                                                >
                                                        linked
                                                </Button>
                                        ) : null}
                                        {mediaMode ? (
                                                <Button
                                                        className="secondary runtime-media-reload"
                                                        variant="secondary"
                                                        type="button"
                                                        title="reload media graph"
                                                        aria-label="reload media graph"
                                                        icon={reloadIcon}
                                                        iconOnly={true}
                                                        disabled={mediaLoading || !mediaDevice}
                                                        onMouseDown={(event) => event.stopPropagation()}
                                                        onClick={() => void loadMediaGraph()}
                                                />
                                        ) : null}
                                </div>
                                <div className="runtime-header-right">
                                        <Button
                                                className={`secondary runtime-device-tree-toggle${deviceTreeMode ? ' active' : ''}`}
                                                variant="secondary"
                                                compact={true}
                                                type="button"
                                                title="show device tree"
                                                aria-label="show device tree"
                                                onMouseDown={(event) => event.stopPropagation()}
                                                onClick={() => {
                                                        setDeviceTreeMode((current) => !current);
                                                        setMediaMode(false);
                                                        setModuleDebugMode(false);
                                                        onSelectMediaElement?.(null);
                                                }}
                                        >
                                                DT
                                        </Button>
                                        <Button
                                                className={`secondary runtime-media-toggle${mediaMode ? ' active' : ''}`}
                                                variant="secondary"
                                                type="button"
                                                title="show media graph"
                                                aria-label="show media graph"
                                                icon={mediaGraphIcon}
                                                iconOnly={true}
                                                onMouseDown={(event) => event.stopPropagation()}
                                                onClick={() => {
                                                        setMediaMode((current) => !current);
                                                        setDeviceTreeMode(false);
                                                        setModuleDebugMode(false);
                                                        if (mediaMode) onSelectMediaElement?.(null);
                                                }}
                                        />
                                        <Button
                                                className={`secondary runtime-module-debug-toggle${moduleDebugMode ? ' active' : ''}`}
                                                variant="secondary"
                                                type="button"
                                                title="show kernel module debug levels"
                                                aria-label="show kernel module debug levels"
                                                icon={moduleDebugIcon}
                                                iconOnly={true}
                                                onMouseDown={(event) => event.stopPropagation()}
                                                onClick={() => {
                                                        setModuleDebugMode((current) => !current);
                                                        setDeviceTreeMode(false);
                                                        setMediaMode(false);
                                                        if (moduleDebugMode) onSelectMediaElement?.(null);
                                                }}
                                        />
                                        <Button
                                                className={`secondary runtime-log-toggle${logOpen ? ' active' : ''}`}
                                                variant="secondary"
                                                type="button"
                                                title="show logs"
                                                aria-label="show logs"
                                                icon={logIcon}
                                                iconOnly={true}
                                                onMouseDown={(event) => event.stopPropagation()}
                                                onClick={() => {
                                                        const nextOpen = !logOpen;
                                                        onToggleLogOpen(runtime.id, nextOpen);
                                                        if (!nextOpen) {
                                                                setFilterOpen(false);
                                                        }
                                                }}
                                        />
                                        <Button
                                                className="runtime-run"
                                                compact={true}
                                                type="button"
                                                onMouseDown={(event) => event.stopPropagation()}
                                                onClick={() => onStartRuntime(runtime.id, runtime.status)}
                                        >
                                                {runtime.status === 'running' ? 'Stop' : 'Start'}
                                        </Button>
                                </div>
                        </header>
                        <div
                                ref={runtimeCanvasRef}
                                className={`runtime-canvas${mediaMode ? ' media-mode' : ''}${deviceTreeMode ? ' device-tree-mode' : ''}${moduleDebugMode ? ' module-debug-mode' : ''}`}
                                onWheelCapture={onRuntimeWheelCapture}
                                onMouseDownCapture={onRuntimePanMouseDownCapture}
                                onMouseUpCapture={onRuntimeContextMenuMouseUpCapture}
                                onContextMenu={(event) => {
                                        event.preventDefault();
                                        event.stopPropagation();
                                }}
                        >
                                <div className="runtime-canvas-tools">
                                        {deviceTreeMode || moduleDebugMode ? null : (
                                                <ResetViewButton
                                                        onClick={resetOrFitRuntimeViewport}
                                                        className="runtime-reset-view-button"
                                                        ariaLabel={`reset view for ${runtime.displayName || runtime.ip}`}
                                                />
                                        )}
                                </div>
                                <div
                                        className={`runtime-canvas-inner${mediaMode ? ' media-mode' : ''}${deviceTreeMode ? ' device-tree-mode' : ''}${moduleDebugMode ? ' module-debug-mode' : ''}`}
                                        style={mediaMode || deviceTreeMode || moduleDebugMode ? undefined : { transform: `translate(${runtimePanX}px, ${runtimePanY}px) scale(${runtimeZoom})` }}
                                >
                                        {deviceTreeMode ? (
                                                <DeviceTreeView
                                                        tree={deviceTree}
                                                        loading={deviceTreeLoading}
                                                        error={deviceTreeError}
                                                        query={deviceTreeQuery}
                                                        onOpenContextMenu={onDeviceTreeContextMenu}
                                                        onStatusChange={onDeviceTreeStatus}
                                                />
                                        ) : moduleDebugMode ? (
                                                <ModuleDebugView
                                                        modules={moduleDebugList}
                                                        loading={moduleDebugLoading}
                                                        error={moduleDebugError}
                                                        drafts={moduleDebugDrafts}
                                                        applyingModules={moduleDebugApplying}
                                                        onDraftChange={onModuleDebugDraftChange}
                                                        onCommit={onModuleDebugCommit}
                                                        query={moduleDebugQuery}
                                                        onStatusChange={onModuleDebugStatus}
                                                />
                                        ) : mediaMode ? (
                                                <MediaGraphView
                                                        graph={mediaGraph}
                                                        loading={mediaLoading}
                                                        error={mediaError}
                                                        connectedOnly={mediaConnectedOnly}
                                                        selectedElement={selectedMediaElement}
                                                        onSelectElement={onSelectMediaElement}
                                                        zoom={runtimeZoom}
                                                        panX={runtimePanX}
                                                        panY={runtimePanY}
                                                />
                                        ) : <>
                                                {runtime.nodes.map((node) => (
                                                        <NodeCard
                                                                key={node.id}
                                                                node={node}
                                                                selected={selectedNodeId === node.id}
                                                                onSelect={onSelectNode}
                                                                onRename={(nextName) => runtime.onRenameNode?.(node.id, nextName)}
                                                                onDragStart={(event) => onNodeDragStart(event, runtime.id, node.id)}
                                                                onContextMenu={(event, targetNode) => onNodeContextMenu(event, runtime.id, targetNode)}
                                                                onPortContextMenu={(event, targetNode, direction) => onNodePortContextMenu(event, runtime.id, targetNode, direction)}
                                                                onPortMouseDown={(event, targetNode, portName) => onNodePortMouseDown(event, runtime.id, targetNode, portName)}
                                                        />
                                                ))}
                                                <EdgeLayer runtime={runtime} onEdgePath={onEdgePath} onEdgeContextMenu={onEdgeContextMenu} />
                                        </>}
                                </div>
                        </div>
                        <RuntimeLogConsole
                                runtimeId={runtime.id}
                                runtimeName={runtime.displayName || runtime.ip}
                                entries={logEntries}
                                open={logOpen}
                                runtimeHeight={runtime.rect?.h || 0}
                                initialConsoleHeight={logConsoleHeight}
                                filterOpen={filterOpen}
                                filterFocusRequest={logFilterOpenRequest?.runtimeId === runtime.id ? logFilterOpenRequest.sequence : 0}
                                filterActive={filterActive}
                                visibleSources={visibleSources}
                                kernelRegexText={kernelRegexText}
                                onSetKernelRegexText={setKernelRegexText}
                                onToggleSource={(sourceId) => {
                                        setVisibleSources((current) => ({ ...current, [sourceId]: current[sourceId] === false }));
                                }}
                                onToggleFilterOpen={() => setFilterOpen((current) => !current)}
                                showDebugDetails={showDebugDetails}
                                onToggleDebugDetails={() => setShowDebugDetails((current) => !current)}
                                logFontSize={logFontSize}
                                canIncreaseFont={logFontSize < LOG_FONT_MAX}
                                canDecreaseFont={logFontSize > LOG_FONT_MIN}
                                onIncreaseFont={() => setLogFontSize((current) => clampLogFontSize(current + 1))}
                                onDecreaseFont={() => setLogFontSize((current) => clampLogFontSize(current - 1))}
                                onConsoleHeightChange={(nextHeight) => setLogConsoleHeight(clampLogConsoleHeight(nextHeight))}
                                onClearLogs={onClearRuntimeLogs}
                        />
                        <div
                                className="runtime-resize-grip"
                                role="presentation"
                                onMouseDown={(event) => {
                                        if (event.button !== 0) {
                                                return;
                                        }
                                        event.preventDefault();
                                        event.stopPropagation();
                                        onRuntimeResizeStart(event, runtime.id);
                                }}
                        />
                </section>
        );
}
