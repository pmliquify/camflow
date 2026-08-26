import React, { useEffect, useMemo } from 'react';
import ScrollArea from '../../components/ScrollArea.jsx';
import Slider from '../../components/Slider.jsx';
import Input from '../../components/Input.jsx';

const DEBUG_SLIDER_MIN = 0;
const DEBUG_SLIDER_MAX = 8;

function parseDebugValue(rawValue) {
        const parsed = Number.parseInt(rawValue, 10);
        return Number.isFinite(parsed) ? parsed : 0;
}

function ModuleDebugRow({ module, draftValue, applying, onDraftChange, onCommit }) {
        const sliderValue = Math.min(DEBUG_SLIDER_MAX, Math.max(DEBUG_SLIDER_MIN, parseDebugValue(draftValue)));
        const readOnly = !module.writable;

        return (
                <div className="module-debug-row">
                        <div className="module-debug-info">
                                <div className="module-debug-title">
                                        <strong className="module-debug-name">{module.name}</strong>
                                        <span className="module-debug-path" title={module.path}>{module.path}</span>
                                </div>
                                <div className="module-debug-meta">
                                        {module.version ? <span>version {module.version}</span> : null}
                                        {module.initstate ? <span>{module.initstate}</span> : null}
                                        {module.refcnt ? <span>refcnt {module.refcnt}</span> : null}
                                        {(module.parameters || []).length > 0 ? (
                                                <span title={module.parameters.join(', ')}>{module.parameters.length} other param{module.parameters.length === 1 ? '' : 's'}</span>
                                        ) : null}
                                        {readOnly ? <span className="module-debug-readonly">read-only</span> : null}
                                </div>
                        </div>
                        <div className="numeric-control module-debug-control">
                                <Slider
                                        min={DEBUG_SLIDER_MIN}
                                        max={DEBUG_SLIDER_MAX}
                                        step="1"
                                        value={sliderValue}
                                        disabled={readOnly || applying}
                                        aria-label={`${module.name} debug level`}
                                        onChange={(event) => onDraftChange(event.target.value)}
                                        onMouseUp={onCommit}
                                        onKeyUp={(event) => {
                                                if (event.key === 'ArrowLeft' || event.key === 'ArrowRight') {
                                                        onCommit();
                                                }
                                        }}
                                />
                                <Input
                                        type="number"
                                        step="1"
                                        value={draftValue ?? ''}
                                        disabled={readOnly || applying}
                                        aria-label={`${module.name} debug level value`}
                                        onChange={(event) => onDraftChange(event.target.value)}
                                        onFocus={(event) => event.currentTarget.select()}
                                        onBlur={onCommit}
                                        onKeyDown={(event) => {
                                                if (event.key === 'Enter') {
                                                        event.preventDefault();
                                                        event.currentTarget.blur();
                                                }
                                        }}
                                />
                        </div>
                </div>
        );
}

export default function ModuleDebugView({ modules, loading, error, drafts, applyingModules, onDraftChange, onCommit, query = '', onStatusChange }) {
        const search = useMemo(() => {
                const text = query.trim();
                if (!text) {
                        return { test: null, invalid: false };
                }
                try {
                        return { test: new RegExp(text, 'i'), invalid: false };
                } catch (_) {
                        return { test: null, invalid: true };
                }
        }, [query]);

        const filteredModules = useMemo(() => {
                if (!search.test) {
                        return modules || [];
                }
                return (modules || []).filter((module) => search.test.test(module.name) || search.test.test(module.path));
        }, [modules, search]);

        const statusText = search.invalid
                ? 'invalid regex'
                : search.test
                        ? `${filteredModules.length} match${filteredModules.length === 1 ? '' : 'es'}`
                        : `${(modules || []).length} modules`;

        useEffect(() => {
                onStatusChange?.(statusText, search.invalid);
        }, [onStatusChange, search.invalid, statusText]);

        if (loading) return <div className="module-debug-state">loading kernel modules</div>;
        if (error) return <div className="module-debug-state is-error">{error}</div>;
        if (!modules || modules.length === 0) return <div className="module-debug-state">no modules expose a debug parameter</div>;
        if (filteredModules.length === 0) return <div className="module-debug-state">no matches</div>;

        return (
                <div className="module-debug-panel" onMouseDown={(event) => event.stopPropagation()}>
                        <ScrollArea className="module-debug-scroll parameter-scroll-area">
                                <div className="module-debug-list">
                                        {filteredModules.map((module) => (
                                                <ModuleDebugRow
                                                        key={module.name}
                                                        module={module}
                                                        draftValue={drafts[module.name] ?? module.value}
                                                        applying={Boolean(applyingModules[module.name])}
                                                        onDraftChange={(value) => onDraftChange(module.name, value)}
                                                        onCommit={() => onCommit(module.name)}
                                                />
                                        ))}
                                </div>
                        </ScrollArea>
                </div>
        );
}
