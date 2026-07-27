import React, { useEffect, useMemo, useState } from 'react';

function normalizeGroups(groups) {
        return (Array.isArray(groups) ? groups : []).map((group) => ({
                id: String(group?.id || ''),
                label: String(group?.label || group?.id || ''),
                items: Array.isArray(group?.items) ? group.items : []
        })).filter((group) => group.id);
}

function itemClassName(item, extra = '') {
        return ['context-menu-item', item?.danger ? 'context-menu-item-danger' : '', extra].filter(Boolean).join(' ');
}

export default function StandardContextMenu({
        open,
        x = 0,
        y = 0,
        onClose,
        groups = [],
        items = [],
        actions = [],
        initialGroupId = '',
        groupAriaLabel = 'menu groups',
        className = '',
        emptyLabel = 'empty'
}) {
        const normalizedGroups = useMemo(() => normalizeGroups(groups), [groups]);
        const [activeGroupId, setActiveGroupId] = useState('');

        useEffect(() => {
                if (!open) {
                        return;
                }
                const preferred = normalizedGroups.find((group) => group.id === initialGroupId && group.items.length > 0);
                const firstAvailable = normalizedGroups.find((group) => group.items.length > 0);
                setActiveGroupId((preferred || firstAvailable || normalizedGroups[0] || { id: '' }).id);
        }, [initialGroupId, normalizedGroups, open]);

        if (!open) {
                return null;
        }

        const activeGroup = normalizedGroups.find((group) => group.id === activeGroupId) || normalizedGroups[0] || null;
        const activeItems = activeGroup?.items || [];
        const hasSubmenu = normalizedGroups.length > 0;
        const simpleItems = Array.isArray(items) ? items : [];
        const footerActions = Array.isArray(actions) ? actions : [];

        return (
                <div className="context-backdrop" onClick={onClose} onContextMenu={(event) => { event.preventDefault(); onClose?.(); }}>
                        <div className={`context-menu${className ? ` ${className}` : ''}`} style={{ left: x, top: y }}>
                                {hasSubmenu ? (
                                        <div className="context-submenu-shell">
                                                <ul className="context-group-list" role="menu" aria-label={groupAriaLabel}>
                                                        {normalizedGroups.map((group) => {
                                                                const hasEntries = group.items.length > 0;
                                                                return (
                                                                        <li key={group.id}>
                                                                                <button
                                                                                        type="button"
                                                                                        className={itemClassName({}, `context-menu-item-parent${activeGroupId === group.id ? ' active' : ''}`)}
                                                                                        onMouseEnter={() => setActiveGroupId(group.id)}
                                                                                        onFocus={() => setActiveGroupId(group.id)}
                                                                                        disabled={!hasEntries}
                                                                                >
                                                                                        <span>{group.label}</span>
                                                                                        <span className="context-parent-indicator">&rsaquo;</span>
                                                                                </button>
                                                                        </li>
                                                                );
                                                        })}
                                                </ul>
                                                <ul className="context-submenu-list" role="menu" aria-label={activeGroup?.label || 'submenu'}>
                                                        {activeItems.length > 0 ? activeItems.map((item) => (
                                                                <li key={String(item?.id || item?.label || '')}>
                                                                        <button
                                                                                type="button"
                                                                                className={itemClassName(item)}
                                                                                disabled={Boolean(item?.disabled)}
                                                                                onClick={() => item?.onSelect?.()}
                                                                        >
                                                                                {item?.label}
                                                                        </button>
                                                                </li>
                                                        )) : <li className="context-menu-empty">{emptyLabel}</li>}
                                                </ul>
                                        </div>
                                ) : (
                                        <ul className="context-submenu-list" role="menu" aria-label="menu">
                                                {simpleItems.map((item) => (
                                                        <li key={String(item?.id || item?.label || '')}>
                                                                <button
                                                                        type="button"
                                                                        className={itemClassName(item)}
                                                                        disabled={Boolean(item?.disabled)}
                                                                        onClick={() => item?.onSelect?.()}
                                                                >
                                                                        {item?.label}
                                                                </button>
                                                        </li>
                                                ))}
                                        </ul>
                                )}
                                {footerActions.length > 0 ? <div className="menu-divider" /> : null}
                                {footerActions.map((action) => (
                                        <button
                                                key={String(action?.id || action?.label || '')}
                                                type="button"
                                                className={itemClassName(action)}
                                                disabled={Boolean(action?.disabled)}
                                                onClick={() => action?.onSelect?.()}
                                        >
                                                {action?.label}
                                        </button>
                                ))}
                        </div>
                </div>
        );
}
