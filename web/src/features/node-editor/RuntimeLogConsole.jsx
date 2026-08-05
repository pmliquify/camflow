import React, { useEffect, useMemo, useRef, useState } from 'react';
import ScrollArea from '../../components/ScrollArea.jsx';
import filterIcon from '../../assets/images/icon-filter.svg';
import clearLogIcon from '../../assets/images/icon-clear-log.svg';
import closeIcon from '../../assets/images/icon-close.svg';
import debugIcon from '../../assets/images/icon-debug-bug.svg';
import Button from '../../components/Button.jsx';
import Input from '../../components/Input.jsx';

const SOURCE_OPTIONS = [
        { id: 'runtime', label: 'runtime' },
        { id: 'node', label: 'node' },
        { id: 'api', label: 'api' },
        { id: 'kernel', label: 'kernel' }
];

const MIN_HEIGHT = 112;
const DEFAULT_HEIGHT = 154;
const MAX_HEIGHT = 320;

function extractKernelTag(text) {
        const match = String(text || '').match(/\bkernel\s*:\s*\d+/i);
        return match ? match[0].toLowerCase().replace(/\s*:\s*/, ':') : '';
}

function stripDebugPrefix(text) {
        let cleaned = String(text || '');
        cleaned = cleaned.replace(/^\s*\d{4}-\d{2}-\d{2}\s+\d{2}:\d{2}:\d{2}\.\d{3}\s*/, '');
        cleaned = cleaned.replace(/^\s*\[[^\]]+\]\s*/, '');
        cleaned = cleaned.replace(/^\s*\[[A-Z]+\]\s*/i, '');
        cleaned = cleaned.replace(/^\s*kernel\s*:\s*\d+\s*/i, '');
        cleaned = cleaned.replace(/\s*\(\s*kernel\s*:\s*\d+\s*\)\s*$/i, '');
        cleaned = cleaned.replace(/^\s*\([^)]*:\d+\)\s*/, '');
        return cleaned.trim();
}

function firstNonEmpty(...values) {
        for (const value of values) {
                const text = String(value || '').trim();
                if (text) {
                        return text;
                }
        }
        return '';
}

function formatSourceLocation(entry) {
        const fileRaw = firstNonEmpty(entry?.file, entry?.filename, entry?.sourceFile, entry?.source_file);
        const slashPos = Math.max(fileRaw.lastIndexOf('/'), fileRaw.lastIndexOf('\\'));
        const file = slashPos >= 0 ? fileRaw.slice(slashPos + 1) : fileRaw;
        const line = Number(entry?.line || entry?.lineNo || entry?.sourceLine || entry?.source_line || 0);
        if (file && Number.isFinite(line) && line > 0) {
                return `${file}:${line}`;
        }
        if (file) {
                return file;
        }
        return firstNonEmpty(entry?.location, entry?.codeLine);
}

function formatTimestamp(timestampMs) {
        const value = Number(timestampMs || 0);
        if (!Number.isFinite(value) || value <= 0) {
                return '';
        }
        const d = new Date(value);
        const yyyy = String(d.getFullYear());
        const mo = String(d.getMonth() + 1).padStart(2, '0');
        const dd = String(d.getDate()).padStart(2, '0');
        const hh = String(d.getHours()).padStart(2, '0');
        const mm = String(d.getMinutes()).padStart(2, '0');
        const ss = String(d.getSeconds()).padStart(2, '0');
        const mmm = String(d.getMilliseconds()).padStart(3, '0');
        return `${yyyy}-${mo}-${dd} ${hh}:${mm}:${ss}.${mmm}`;
}

function buildDebugLine(entry) {
        const message = String(entry?.message || entry?.rendered || '');
        const core = stripDebugPrefix(message);
        const source = String(entry?.source || 'runtime').toLowerCase();
        const type = String(entry?.type || 'info').toUpperCase();
        const ts = formatTimestamp(entry?.timestampMs);

        if (source === 'runtime' || source === 'node' || source === 'api') {
                const location = formatSourceLocation(entry);
                const prefix = [ts, `[${type}]`].filter(Boolean).join(' ');
                if (!location) {
                        return core ? `${prefix} ${core}`.trim() : prefix;
                }
                return `${prefix} ${core} (${location})`.trim();
        }

        const kernelTag = source === 'kernel'
                ? firstNonEmpty(entry?.sourceTag, extractKernelTag(message), 'kernel:0').toLowerCase().replace(/\s*:\s*/, ':')
                : source;
        const prefix = [ts, `[${type}]`].filter(Boolean).join(' ');
        if (!core) {
                return `${prefix} (${kernelTag})`.trim();
        }
        return `${prefix} ${core} (${kernelTag})`.trim();
}

function onLogCopyShortcut(event) {
        if (event.key.toLowerCase() !== 'c' || (!event.ctrlKey && !event.metaKey)) {
                return;
        }
        const selectedText = String(window.getSelection?.()?.toString?.() || '');
        if (!selectedText) {
                return;
        }

        // Keep native copy behavior when available, but provide a clipboard fallback.
        if (navigator.clipboard?.writeText) {
                event.preventDefault();
                void navigator.clipboard.writeText(selectedText).catch(() => {
                        // Ignore clipboard permission/runtime failures.
                });
        }
}

function RuntimeLogConsole({
        runtimeId,
        runtimeName,
        entries,
        open,
        runtimeHeight = 0,
        initialConsoleHeight = DEFAULT_HEIGHT,
        filterOpen,
        filterActive,
        visibleSources,
        kernelRegexText,
        onSetKernelRegexText,
        onToggleSource,
        onToggleFilterOpen,
        showDebugDetails = false,
        onToggleDebugDetails,
        logFontSize = 12,
        canIncreaseFont = true,
        canDecreaseFont = true,
        onIncreaseFont,
        onDecreaseFont,
        onConsoleHeightChange,
        onClearLogs
}) {
        const [consoleHeight, setConsoleHeight] = useState(() => Math.max(MIN_HEIGHT, Math.min(MAX_HEIGHT, Number(initialConsoleHeight) || DEFAULT_HEIGHT)));
        const scrollAreaRef = useRef(null);
        const stickToBottomRef = useRef(true);

        const compiledKernelRegex = useMemo(() => {
                const text = kernelRegexText.trim();
                if (!text) {
                        return null;
                }
                try {
                        return new RegExp(text, 'i');
                } catch (_) {
                        return null;
                }
        }, [kernelRegexText]);

        const kernelRegexInvalid = Boolean(kernelRegexText.trim()) && compiledKernelRegex === null;

        const displayedEntries = useMemo(() => {
                return (entries || []).filter((entry) => {
                        const source = entry?.source || 'runtime';
                        if (!visibleSources[source]) {
                                return false;
                        }
                        if (source !== 'kernel') {
                                return true;
                        }
                        if (!compiledKernelRegex) {
                                return true;
                        }
                        const haystack = String(entry?.message || entry?.rendered || '');
                        return compiledKernelRegex.test(haystack);
                });
        }, [compiledKernelRegex, entries, kernelRegexInvalid, visibleSources]);

        const maxHeight = Math.max(MIN_HEIGHT, Math.min(MAX_HEIGHT, Math.max(0, runtimeHeight - 94) || MAX_HEIGHT));
        const currentHeight = Math.max(MIN_HEIGHT, Math.min(consoleHeight, maxHeight));

        useEffect(() => {
                if (!Number.isFinite(Number(initialConsoleHeight))) {
                        return;
                }
                const normalized = Math.max(MIN_HEIGHT, Math.min(MAX_HEIGHT, Number(initialConsoleHeight)));
                setConsoleHeight((current) => (current === normalized ? current : normalized));
        }, [initialConsoleHeight]);

        useEffect(() => {
                if (!open) {
                        stickToBottomRef.current = true;
                }
        }, [open]);

        useEffect(() => {
                const element = scrollAreaRef.current;
                if (!element || !stickToBottomRef.current) {
                        return;
                }
                element.scrollTop = element.scrollHeight;
        }, [displayedEntries.length, open]);

        if (!open) {
                return null;
        }

        return (
                <section className="runtime-log-console" style={{ height: `${currentHeight}px` }} data-runtime-id={runtimeId}>
                        <div
                                className="runtime-log-resize-handle"
                                role="separator"
                                aria-label={`resize log console for ${runtimeName || runtimeId}`}
                                onMouseDown={(event) => {
                                        if (event.button !== 0) {
                                                return;
                                        }
                                        event.preventDefault();
                                        event.stopPropagation();

                                        const startY = event.clientY;
                                        const startHeight = currentHeight;
                                        const currentMaxHeight = maxHeight;

                                        const moveHandler = (moveEvent) => {
                                                const deltaY = startY - moveEvent.clientY;
                                                const nextHeight = Math.max(MIN_HEIGHT, Math.min(currentMaxHeight, startHeight + deltaY));
                                                setConsoleHeight(nextHeight);
                                                if (onConsoleHeightChange) {
                                                        onConsoleHeightChange(nextHeight);
                                                }
                                        };

                                        const stopHandler = () => {
                                                window.removeEventListener('mousemove', moveHandler);
                                                window.removeEventListener('mouseup', stopHandler);
                                                window.removeEventListener('blur', stopHandler);
                                        };

                                        window.addEventListener('mousemove', moveHandler);
                                        window.addEventListener('mouseup', stopHandler);
                                        window.addEventListener('blur', stopHandler);
                                }}
                        />
                        <div className="runtime-log-body">
                                <div className="runtime-log-main">
                                        {filterOpen ? (
                                                <div className="runtime-log-filter-panel">
                                                        <div className="runtime-log-source-toggle-row">
                                                                {SOURCE_OPTIONS.map((option) => {
                                                                        const active = visibleSources[option.id] !== false;
                                                                        return (
                                                                                <React.Fragment key={option.id}>
                                                                                        <Button
                                                                                                className={`secondary runtime-log-source-toggle${active ? ' active' : ''}`}
                                                                                                variant="secondary"
                                                                                                type="button"
                                                                                                onMouseDown={(event) => event.stopPropagation()}
                                                                                                onClick={() => onToggleSource(option.id)}
                                                                                        >
                                                                                                {option.label}
                                                                                        </Button>
                                                                                        {option.id === 'kernel' ? (
                                                                                                <>
                                                                                                        <div className="runtime-log-search-field inline-runtime-log-search-field">
                                                                                                                <Input
                                                                                                                        type="text"
                                                                                                                        value={kernelRegexText}
                                                                                                                        placeholder="kernel regex"
                                                                                                                        onMouseDown={(event) => event.stopPropagation()}
                                                                                                                        onChange={(event) => onSetKernelRegexText(event.target.value)}
                                                                                                                />
                                                                                                                <Button
                                                                                                                        className={`runtime-log-search-clear${kernelRegexText ? '' : ' is-empty'}`}
                                                                                                                        type="button"
                                                                                                                        title="clear regex"
                                                                                                                        icon={closeIcon}
                                                                                                                        iconOnly={true}
                                                                                                                        onMouseDown={(event) => event.stopPropagation()}
                                                                                                                        onClick={() => onSetKernelRegexText('')}
                                                                                                                />
                                                                                                        </div>
                                                                                                        <Button
                                                                                                                className={`runtime-log-debug-toggle${showDebugDetails ? ' active' : ''}`}
                                                                                                                variant="secondary"
                                                                                                                type="button"
                                                                                                                title="toggle debug details"
                                                                                                                aria-label="toggle debug details"
                                                                                                                icon={debugIcon}
                                                                                                                iconOnly={true}
                                                                                                                onMouseDown={(event) => event.stopPropagation()}
                                                                                                                onClick={onToggleDebugDetails}
                                                                                                        />
                                                                                                </>
                                                                                        ) : null}
                                                                                </React.Fragment>
                                                                        );
                                                                })}
                                                        </div>
                                                        {kernelRegexInvalid ? <div className="runtime-log-filter-error">invalid regex</div> : null}
                                                </div>
                                        ) : null}
                                        <ScrollArea
                                                ref={scrollAreaRef}
                                                className="runtime-log-scroll-area parameter-scroll-area"
                                                style={{ '--runtime-log-font-size': `${logFontSize}px` }}
                                                tabIndex={0}
                                                onMouseDown={(event) => event.stopPropagation()}
                                                onKeyDown={onLogCopyShortcut}
                                                stopWheelPropagation={true}
                                                transientScrollbar={false}
                                                onScroll={(event) => {
                                                        const element = event.currentTarget;
                                                        const distanceToBottom = element.scrollHeight - (element.scrollTop + element.clientHeight);
                                                        stickToBottomRef.current = distanceToBottom <= 8;
                                                }}
                                        >
                                                {displayedEntries.length > 0 ? displayedEntries.map((entry, index) => {
                                                        const source = entry?.source || 'runtime';
                                                        const text = showDebugDetails
                                                                ? buildDebugLine(entry)
                                                                : stripDebugPrefix(String(entry?.message || entry?.rendered || ''));
                                                        return (
                                                                <div key={entry?.__uiKey || `${entry?.timestampMs || index}-${index}`} className={`runtime-log-line source-${source} type-${entry?.type || 'info'}`}>
                                                                        <span className="runtime-log-text">{text}</span>
                                                                </div>
                                                        );
                                                }) : <div className="runtime-log-empty">waiting for log stream</div>}
                                        </ScrollArea>
                                </div>
                                <div className="runtime-log-actions-strip" onMouseDown={(event) => event.stopPropagation()}>
                                        <Button
                                                className={`runtime-log-icon-button${filterOpen || filterActive ? ' active-filter' : ''}`}
                                                type="button"
                                                title="filter logs"
                                                aria-label="filter logs"
                                                icon={filterIcon}
                                                iconOnly={true}
                                                onClick={onToggleFilterOpen}
                                        />
                                        <Button
                                                className="runtime-log-icon-button"
                                                type="button"
                                                title="clear logs"
                                                aria-label="clear logs"
                                                icon={clearLogIcon}
                                                iconOnly={true}
                                                onClick={() => onClearLogs(runtimeId)}
                                        />
                                        <Button
                                                className="runtime-log-icon-button runtime-log-font-button font-large"
                                                type="button"
                                                title="increase log font"
                                                aria-label="increase log font"
                                                disabled={!canIncreaseFont}
                                                onClick={onIncreaseFont}
                                        >
                                                A+
                                        </Button>
                                        <Button
                                                className="runtime-log-icon-button runtime-log-font-button font-small"
                                                type="button"
                                                title="decrease log font"
                                                aria-label="decrease log font"
                                                disabled={!canDecreaseFont}
                                                onClick={onDecreaseFont}
                                        >
                                                A-
                                        </Button>
                                </div>
                        </div>
                </section>
        );
}

export default React.memo(RuntimeLogConsole);
