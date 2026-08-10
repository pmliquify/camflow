import React, { useEffect, useMemo, useState } from 'react';
import RuntimeWindowIcon from './RuntimeWindowIcon.jsx';
import NodeCard from './NodeCard.jsx';
import EdgeLayer from './EdgeLayer.jsx';
import RuntimeLogConsole from './RuntimeLogConsole.jsx';
import MediaGraphView from './MediaGraphView.jsx';
import logIcon from '../../assets/images/icon-log.svg';
import mediaGraphIcon from '../../assets/images/icon-media-graph.svg';
import reloadIcon from '../../assets/images/icon-reload.svg';
import Button from '../../components/Button.jsx';
import InlineNameEditor from '../../components/InlineNameEditor.jsx';
import { getMediaDevices, getMediaGraph } from '../../services/runtimeApi.js';

const RUNTIME_LOG_PREFS_STORAGE_PREFIX = 'camflow:runtime-log-prefs:';
const RUNTIME_LOG_DEFAULT_FONT_STORAGE_KEY = 'camflow:runtime-log-default-font-size';
const RUNTIME_MEDIA_PREFS_STORAGE_PREFIX = 'camflow:runtime-media-prefs:';
const LOG_FONT_MIN = 5;
const LOG_FONT_MAX = 15;
const LOG_FONT_DEFAULT = 10;
const LOG_CONSOLE_MIN_HEIGHT = 112;
const LOG_CONSOLE_MAX_HEIGHT = 320;
const LOG_CONSOLE_DEFAULT_HEIGHT = 154;

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

export default function RuntimeLane({
        runtime,
        logEntries,
        logOpen,
        onToggleLogOpen,
        selectedNodeId,
        onSelectNode,
        onLaneContextMenu,
        onNodeContextMenu,
        onEdgePath,
        onDeleteEdge,
        onNodeDragStart,
        onNodePortAreaClick,
        onNodePortMouseDown,
        onHideNodePort,
        onRuntimeDragStart,
        onRuntimeResizeStart,
        onStartRuntime,
        onClearRuntimeLogs,
        runtimeBaseUrl = '',
        selectedMediaElement,
        onSelectMediaElement
}) {
        const initialLogPrefs = useMemo(() => loadRuntimeLogPrefs(runtime.id), [runtime.id]);
        const initialMediaPrefs = useMemo(() => loadRuntimeMediaPrefs(runtime.id), [runtime.id]);
        const [filterOpen, setFilterOpen] = useState(() => initialLogPrefs?.filterOpen || false);
        const [visibleSources, setVisibleSources] = useState(() => initialLogPrefs?.visibleSources || { kernel: true, runtime: true, node: true, api: true });
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

        const filterActive = useMemo(() => {
                return Object.values(visibleSources).some((value) => !value) || Boolean(kernelRegexText.trim());
        }, [kernelRegexText, visibleSources]);

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

        return (
                <section
                        className="runtime-lane"
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
                        <header onContextMenu={(event) => onLaneContextMenu(event, runtime.id)} onMouseDown={(event) => onRuntimeDragStart(event, runtime.id)}>
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
                                <div className="runtime-header-right">
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
                                                        if (mediaMode) onSelectMediaElement?.(null);
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
                        <div className={`runtime-canvas${mediaMode ? ' media-mode' : ''}`} onContextMenu={(event) => onLaneContextMenu(event, runtime.id)}>
                                {mediaMode ? (
                                        <MediaGraphView
                                                graph={mediaGraph}
                                                loading={mediaLoading}
                                                error={mediaError}
                                                connectedOnly={mediaConnectedOnly}
                                                selectedElement={selectedMediaElement}
                                                onSelectElement={onSelectMediaElement}
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
                                                        onContextMenu={(event) => onNodeContextMenu(event, runtime.id, node.id)}
                                                        onPortAreaClick={(event, targetNode, direction) => onNodePortAreaClick(event, runtime.id, targetNode, direction)}
                                                        onPortMouseDown={(event, targetNode, portName) => onNodePortMouseDown(event, runtime.id, targetNode, portName)}
                                                        onHidePort={onHideNodePort}
                                                />
                                        ))}
                                        <EdgeLayer runtime={runtime} onEdgePath={onEdgePath} onDeleteEdge={onDeleteEdge} />
                                </>}
                        </div>
                        <RuntimeLogConsole
                                runtimeId={runtime.id}
                                runtimeName={runtime.displayName || runtime.ip}
                                entries={logEntries}
                                open={logOpen}
                                runtimeHeight={runtime.rect?.h || 0}
                                initialConsoleHeight={logConsoleHeight}
                                filterOpen={filterOpen}
                                filterActive={filterActive}
                                visibleSources={visibleSources}
                                kernelRegexText={kernelRegexText}
                                onSetKernelRegexText={setKernelRegexText}
                                onToggleSource={(sourceId) => {
                                        setVisibleSources((current) => ({ ...current, [sourceId]: !current[sourceId] }));
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
