import React, { useEffect, useRef, useState } from 'react';
import ParameterRow from './ParameterRow.jsx';
import RuntimeActions from './RuntimeActions.jsx';
import ScrollArea from '../../components/ScrollArea.jsx';
import Button from '../../components/Button.jsx';
import Input from '../../components/Input.jsx';
import filterIcon from '../../assets/images/icon-filter.svg';
import reloadIcon from '../../assets/images/icon-reload.svg';
import closeIcon from '../../assets/images/icon-close.svg';

const PARAM_FILTER_STORAGE_KEY = 'camflow:param-filter:v1';

function readStoredParameterFilter() {
        try {
                const raw = window.localStorage.getItem(PARAM_FILTER_STORAGE_KEY);
                if (!raw) {
                        return { text: '', visibleByName: {} };
                }
                const parsed = JSON.parse(raw);
                const text = typeof parsed?.text === 'string' ? parsed.text : '';
                const visibleByName = parsed && typeof parsed.visibleByName === 'object' && parsed.visibleByName !== null ? parsed.visibleByName : {};
                return { text, visibleByName };
        } catch (_) {
                return { text: '', visibleByName: {} };
        }
}

function parameterDisplayName(item) {
        const fullName = String(item?.name || '');
        const groupName = String(item?.group || '').trim().toLowerCase();
        const groupPrefix = groupName ? `${groupName}.` : '';
        return groupPrefix && fullName.startsWith(groupPrefix) ? fullName.slice(groupPrefix.length) : fullName;
}

function mediaDetail(selectedElement) {
        if (!selectedElement?.item) return null;
        const { kind, item, entity, graph } = selectedElement;
        const value = (name, fieldValue, description) => ({ name, type: 'string', value: String(fieldValue ?? ''), description, runtimeWritable: false });
        const flags = (flagKind, fieldValue, description) => ({ name: 'flags', type: 'media-flags', flagKind, value: fieldValue, description, runtimeWritable: false });
        if (kind === 'entity') {
                const totalPadCount = graph?.entities?.find((candidate) => candidate.id === item.id)?.pads?.length ?? item.pads?.length ?? 0;
                return {
                        title: item.name,
                        parameters: [
                                value('function', `${item.function} (0x${Number(item.functionId || 0).toString(16)})`, 'Media entity function and kernel function id'),
                                value('pads', totalPadCount, 'Total number of entity pads'),
                                flags('entity', item.flags, 'Entity flags and their states')
                        ]
                };
        }
        if (kind === 'pad') {
                return {
                        title: `${entity?.name || 'entity'} pad ${item.index}`,
                        parameters: [
                                value('id', item.id, 'Kernel media pad id'),
                                flags('pad', item.flags, 'Pad flags and their states')
                        ]
                };
        }
        const entityName = (id) => graph?.entities?.find((candidate) => candidate.id === id)?.name || id;
        return {
                title: `${entityName(item.sourceEntityId)} -> ${entityName(item.sinkEntityId)}`,
                parameters: [
                        value('id', item.id, 'Kernel media link id'),
                        flags('link', item.flags, 'Link flags and their states')
                ]
        };
}

export default function ParameterPanel({
        selectedNodeMeta,
        selectedRuntimeName,
        onReload,
        selectedRuntimeId,
        localRuntimeId,
        deleteRuntime,
        selectedNodeParams,
        updateParameter,
        runtimeRunning,
        selectedMediaElement,
        filterOpenRequest = 0,
        filterCloseRequest = 0
}) {
        const initialFilter = readStoredParameterFilter();
        const [filterOpen, setFilterOpen] = useState(false);
        const [filterText, setFilterText] = useState(initialFilter.text);
        const [visibleByName, setVisibleByName] = useState(initialFilter.visibleByName);
        const searchInputRef = useRef(null);

        useEffect(() => {
                setVisibleByName((current) => {
                        const next = { ...current };
                        let changed = false;
                        for (const item of selectedNodeParams) {
                                if (next[item.name] === undefined) {
                                        next[item.name] = true;
                                        changed = true;
                                }
                        }
                        return changed ? next : current;
                });
        }, [selectedNodeParams]);

        useEffect(() => {
                try {
                        window.localStorage.setItem(PARAM_FILTER_STORAGE_KEY, JSON.stringify({ text: filterText, visibleByName }));
                } catch (_) {
                        // Ignore storage write failures.
                }
        }, [filterText, visibleByName]);

        useEffect(() => {
                if (filterOpenRequest > 0) {
                        setFilterOpen(true);
                }
        }, [filterOpenRequest]);

        useEffect(() => {
                if (!filterOpen || filterOpenRequest === 0) {
                        return undefined;
                }
                const frameId = window.requestAnimationFrame(() => {
                        searchInputRef.current?.focus();
                        searchInputRef.current?.select();
                });
                return () => window.cancelAnimationFrame(frameId);
        }, [filterOpen, filterOpenRequest]);

        useEffect(() => {
                if (filterCloseRequest > 0) {
                        setFilterOpen(false);
                }
        }, [filterCloseRequest]);

        const selectedMediaDetail = mediaDetail(selectedMediaElement);
        const activeParams = selectedMediaDetail?.parameters || selectedNodeParams;
        const normalizedSearch = filterText.trim().toLowerCase();
        const hasHiddenParameters = !selectedMediaDetail && activeParams.some((item) => (visibleByName[item.name] ?? true) === false);
        const displayedParams = activeParams.filter((item) => {
                if (filterOpen) {
                        if (!normalizedSearch) {
                                return true;
                        }

                        const internalName = String(item.name || '').toLowerCase();
                        const visibleName = parameterDisplayName(item).toLowerCase();
                        const groupName = String(item.group || '').trim().toLowerCase();
                        const groupDescription = String(item.groupDescription || '').trim().toLowerCase();
                        return internalName.includes(normalizedSearch)
                                || visibleName.includes(normalizedSearch)
                                || groupName.includes(normalizedSearch)
                                || groupDescription.includes(normalizedSearch);
                }
                return selectedMediaDetail ? true : (visibleByName[item.name] ?? true) === true;
        });

        const groupedParameterItems = [];
        let lastGroup = '';
        for (const item of displayedParams) {
                const currentGroup = String(item.group || '').trim().toLowerCase();
                if (currentGroup && currentGroup !== lastGroup) {
                        const groupDescription = String(item.groupDescription || '').trim();
                        groupedParameterItems.push({
                                kind: 'group',
                                id: `group:${currentGroup}`,
                                group: currentGroup,
                                label: groupDescription || currentGroup
                        });
                        lastGroup = currentGroup;
                }
                if (!currentGroup) {
                        lastGroup = '';
                }
                groupedParameterItems.push({ kind: 'parameter', id: `parameter:${item.name}`, item });
        }

        function setParamVisibility(name, visible) {
                setVisibleByName((current) => ({ ...current, [name]: visible }));
        }

        function clearFilter() {
                setFilterText('');
                setVisibleByName((current) => {
                        const next = { ...current };
                        let changed = false;
                        for (const item of selectedNodeParams) {
                                if (next[item.name] !== true) {
                                        next[item.name] = true;
                                        changed = true;
                                }
                        }
                        return changed ? next : current;
                });
        }

        function deselectAllParameters() {
                setVisibleByName((current) => {
                        const next = { ...current };
                        let changed = false;
                        for (const item of selectedNodeParams) {
                                if (next[item.name] !== false) {
                                        next[item.name] = false;
                                        changed = true;
                                }
                        }
                        return changed ? next : current;
                });
        }

        return (
                <section className="panel parameter-panel">
                        <div className="parameter-header">
                                <div className="parameter-header-left">
                                        {selectedMediaDetail ? <div className="selection compact-selection media-detail-title">{selectedMediaDetail.title}</div> : selectedNodeMeta ? <div className="selection compact-selection">{selectedNodeMeta.name || selectedNodeMeta.id}</div> : <div className="selection compact-selection">{selectedRuntimeName}</div>}
                                </div>
                                <div className="parameter-header-right">
                                        <Button
                                                className={`secondary param-filter-button${hasHiddenParameters ? ' active-filter' : ''}`}
                                                variant="secondary"
                                                type="button"
                                                icon={filterIcon}
                                                onClick={() => setFilterOpen((open) => !open)}
                                                title="Filter parameters"
                                        >
                                                filter
                                        </Button>
                                        {!selectedMediaDetail ? <Button className="secondary param-reload-button" variant="secondary" type="button" icon={reloadIcon} onClick={onReload} title="Reload parameters">reload</Button> : null}
                                </div>
                        </div>
                        {filterOpen ? (
                                <div className="parameter-filter-panel">
                                        <div className="parameter-search-field">
                                                <Input
                                                        ref={searchInputRef}
                                                        type="text"
                                                        value={filterText}
                                                        placeholder="search by parameter name"
                                                        onChange={(event) => setFilterText(event.target.value)}
                                                />
                                                <Button
                                                        className={`parameter-search-clear${filterText ? '' : ' is-empty'}`}
                                                        type="button"
                                                        title="clear search"
                                                        icon={closeIcon}
                                                        iconOnly={true}
                                                        onClick={() => {
                                                                if (!filterText) {
                                                                        if (searchInputRef.current) {
                                                                                searchInputRef.current.focus();
                                                                        }
                                                                        return;
                                                                }
                                                                setFilterText('');
                                                                if (searchInputRef.current) {
                                                                        searchInputRef.current.focus();
                                                                }
                                                        }}
                                                />
                                        </div>
                                        <div className="parameter-filter-actions">
                                                <Button className="secondary" variant="secondary" type="button" onClick={clearFilter}>clear filter</Button>
                                                <Button className="secondary" variant="secondary" type="button" onClick={deselectAllParameters}>deselect all</Button>
                                        </div>
                                </div>
                        ) : null}
                        {!selectedNodeMeta && selectedRuntimeId !== localRuntimeId ? (
                                <RuntimeActions selectedRuntimeId={selectedRuntimeId} deleteRuntime={deleteRuntime} />
                        ) : null}
                        <ScrollArea className="parameter-scroll-area" stopWheelPropagation={true}>
                                <div className="parameter-list compact-parameters">
                                        {groupedParameterItems.length === 0 ? null : groupedParameterItems.map((entry) => {
                                                if (entry.kind === 'group') {
                                                        return (
                                                                <div key={entry.id} className="parameter-group-separator" title={entry.label}>
                                                                        <span>{entry.label}</span>
                                                                </div>
                                                        );
                                                }

                                                const item = entry.item;
                                                const canEdit = !selectedMediaDetail && (!runtimeRunning || item.runtimeWritable !== false);
                                                return (
                                                        <ParameterRow
                                                                key={entry.id}
                                                                item={item}
                                                                canEdit={canEdit}
                                                                onChange={updateParameter}
                                                                showVisibilityCheckbox={filterOpen}
                                                                parameterVisible={visibleByName[item.name] ?? true}
                                                                onVisibilityChange={setParamVisibility}
                                                        />
                                                );
                                        })}
                                </div>
                        </ScrollArea>
                </section>
        );
}
