import React, { useEffect, useMemo, useState } from 'react';
import RuntimeWindowIcon from './RuntimeWindowIcon.jsx';
import NodeCard from './NodeCard.jsx';
import EdgeLayer from './EdgeLayer.jsx';
import RuntimeLogConsole from './RuntimeLogConsole.jsx';
import logIcon from '../../assets/images/icon-log.svg';
import Button from '../../components/Button.jsx';
import InlineNameEditor from '../../components/InlineNameEditor.jsx';

const RUNTIME_LOG_PREFS_STORAGE_PREFIX = 'camflow:runtime-log-prefs:';
const RUNTIME_LOG_DEFAULT_FONT_STORAGE_KEY = 'camflow:runtime-log-default-font-size';
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
        onClearRuntimeLogs
}) {
        const initialLogPrefs = useMemo(() => loadRuntimeLogPrefs(runtime.id), [runtime.id]);
        const [filterOpen, setFilterOpen] = useState(() => initialLogPrefs?.filterOpen || false);
        const [visibleSources, setVisibleSources] = useState(() => initialLogPrefs?.visibleSources || { kernel: true, runtime: true, node: true, api: true });
        const [kernelRegexText, setKernelRegexText] = useState(() => initialLogPrefs?.kernelRegexText || '');
        const [showDebugDetails, setShowDebugDetails] = useState(() => initialLogPrefs?.showDebugDetails || false);
        const [logFontSize, setLogFontSize] = useState(() => initialLogPrefs?.logFontSize || loadGlobalDefaultLogFontSize());
        const [logConsoleHeight, setLogConsoleHeight] = useState(() => initialLogPrefs?.logConsoleHeight || LOG_CONSOLE_DEFAULT_HEIGHT);

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
                                                type="button"
                                                onMouseDown={(event) => event.stopPropagation()}
                                                onClick={() => onStartRuntime(runtime.id, runtime.status)}
                                        >
                                                {runtime.status === 'running' ? 'Stop' : 'Start'}
                                        </Button>
                                </div>
                        </header>
                        <div className="runtime-canvas" onContextMenu={(event) => onLaneContextMenu(event, runtime.id)}>
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
