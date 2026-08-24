import React, { useEffect, useMemo, useState } from 'react';
import ScrollArea from '../../components/ScrollArea.jsx';

function formatPropertyValue(property) {
        const suffix = property.truncated ? ' …' : '';
        switch (property.type) {
                case 'empty':
                        return '(empty)';
                case 'string':
                case 'stringList':
                        return (property.strings || []).map((value) => `"${value}"`).join(', ') + suffix;
                case 'cells':
                        return `<${(property.cells || []).map((value) => `0x${(value >>> 0).toString(16)}`).join(' ')}>${suffix}`;
                case 'bytes':
                        return `[${((property.bytes || '').match(/../g) || []).join(' ')}]${suffix}`;
                default:
                        return '(unreadable)';
        }
}

function copyText(text) {
        if (navigator.clipboard?.writeText) {
                void navigator.clipboard.writeText(text).catch(() => copyTextFallback(text));
                return;
        }
        copyTextFallback(text);
}

// navigator.clipboard is unavailable over plain HTTP, which is how runtimes are usually reached.
function copyTextFallback(text) {
        const helper = document.createElement('textarea');
        helper.value = text;
        helper.setAttribute('readonly', '');
        helper.style.position = 'fixed';
        helper.style.opacity = '0';
        document.body.appendChild(helper);
        helper.select();
        try {
                document.execCommand('copy');
        } catch (_) {
                // Ignore clipboard failures.
        }
        document.body.removeChild(helper);
}

function decorateNode(node) {
        return {
                ...node,
                valueText: (node.properties || []).map((property) => formatPropertyValue(property)),
                children: (node.children || []).map(decorateNode)
        };
}

function filterNode(node, pattern) {
        const nameMatch = pattern ? pattern.test(node.name) : true;
        const matchedProperties = new Set();
        if (pattern) {
                (node.properties || []).forEach((property, index) => {
                        if (pattern.test(property.name) || pattern.test(node.valueText[index])) {
                                matchedProperties.add(property.name);
                        }
                });
        }
        const children = (node.children || []).map((child) => filterNode(child, pattern)).filter(Boolean);
        if (pattern && !nameMatch && matchedProperties.size === 0 && children.length === 0) {
                return null;
        }
        const keptIndices = [];
        (node.properties || []).forEach((property, index) => {
                if (!pattern || nameMatch || matchedProperties.has(property.name)) {
                        keptIndices.push(index);
                }
        });
        return {
                ...node,
                nameMatch: Boolean(pattern) && nameMatch,
                matchedProperties,
                properties: keptIndices.map((index) => node.properties[index]),
                valueText: keptIndices.map((index) => node.valueText[index]),
                children
        };
}

function countMatches(node) {
        return (node.nameMatch ? 1 : 0) + node.matchedProperties.size + (node.children || []).reduce((total, child) => total + countMatches(child), 0);
}

function Highlight({ text, pattern }) {
        if (!pattern) {
                return <>{text}</>;
        }
        const parts = [];
        let cursor = 0;
        let key = 0;
        for (const match of text.matchAll(pattern)) {
                if (!match[0]) {
                        continue;
                }
                if (match.index > cursor) {
                        parts.push(text.slice(cursor, match.index));
                }
                parts.push(<mark key={key++}>{match[0]}</mark>);
                cursor = match.index + match[0].length;
        }
        if (parts.length === 0) {
                return <>{text}</>;
        }
        parts.push(text.slice(cursor));
        return <>{parts}</>;
}

function DeviceTreeNode({ node, depth, expandedPaths, onToggle, onContextMenu, forceExpand, pattern }) {
        const expanded = forceExpand || expandedPaths.has(node.path);
        const childCount = (node.children || []).length;
        const propertyCount = (node.properties || []).length;
        const expandable = childCount > 0 || propertyCount > 0;
        return (
                <div className="device-tree-node">
                        <button
                                type="button"
                                className={`device-tree-row${node.nameMatch ? ' matched' : ''}`}
                                style={{ paddingLeft: 4 + depth * 12 }}
                                onClick={() => onToggle(node.path)}
                                onContextMenu={(event) => onContextMenu(event, { label: 'copy path', onSelect: () => copyText(node.path) })}
                                title={node.path}
                        >
                                <span className={`device-tree-caret${expandable ? '' : ' leaf'}${expanded ? ' expanded' : ''}`} aria-hidden="true" />
                                <span className="device-tree-name"><Highlight text={node.name} pattern={pattern} /></span>
                                <span className="device-tree-counts">{propertyCount ? `${propertyCount}p` : ''}{propertyCount && childCount ? ' · ' : ''}{childCount ? `${childCount}n` : ''}</span>
                        </button>
                        {expanded ? (
                                <>
                                        {(node.properties || []).map((property, index) => (
                                                <div
                                                        key={property.name}
                                                        className={`device-tree-property${node.matchedProperties.has(property.name) ? ' matched' : ''}`}
                                                        style={{ paddingLeft: 20 + depth * 12 }}
                                                        onContextMenu={(event) => onContextMenu(event, { label: `copy value of ${property.name}`, onSelect: () => copyText(node.valueText[index]) })}
                                                        title={`${node.path}/${property.name}`}
                                                >
                                                        <span className="device-tree-property-name"><Highlight text={property.name} pattern={pattern} /></span>
                                                        <span className="device-tree-property-value"><Highlight text={node.valueText[index]} pattern={pattern} /></span>
                                                </div>
                                        ))}
                                        {(node.children || []).map((child) => (
                                                <DeviceTreeNode
                                                        key={child.path}
                                                        node={child}
                                                        depth={depth + 1}
                                                        expandedPaths={expandedPaths}
                                                        onToggle={onToggle}
                                                        onContextMenu={onContextMenu}
                                                        forceExpand={forceExpand}
                                                        pattern={pattern}
                                                />
                                        ))}
                                </>
                        ) : null}
                </div>
        );
}

export default function DeviceTreeView({ tree, loading, error, query, onOpenContextMenu, onStatusChange }) {
        const [expandedPaths, setExpandedPaths] = useState(() => new Set(['/']));

        const search = useMemo(() => {
                const text = query.trim();
                if (!text) {
                        return { test: null, highlight: null, invalid: false };
                }
                try {
                        return { test: new RegExp(text, 'i'), highlight: new RegExp(text, 'gi'), invalid: false };
                } catch (_) {
                        return { test: null, highlight: null, invalid: true };
                }
        }, [query]);

        const decorated = useMemo(() => (tree?.node ? decorateNode(tree.node) : null), [tree]);
        const displayedRoot = useMemo(() => (decorated ? filterNode(decorated, search.test) : null), [decorated, search]);
        const matchCount = useMemo(() => (search.test && displayedRoot ? countMatches(displayedRoot) : 0), [displayedRoot, search]);
        const statusText = search.invalid
                ? 'invalid regex'
                : search.test
                        ? `${matchCount} match${matchCount === 1 ? '' : 'es'}`
                        : `${tree?.nodeCount || 0} nodes`;

        useEffect(() => {
                setExpandedPaths(new Set(['/']));
        }, [tree]);

        useEffect(() => {
                onStatusChange?.(statusText, search.invalid);
        }, [onStatusChange, search.invalid, statusText]);

        const toggle = (path) => {
                setExpandedPaths((current) => {
                        const next = new Set(current);
                        if (next.has(path)) {
                                next.delete(path);
                        } else {
                                next.add(path);
                        }
                        return next;
                });
        };

        const openContextMenu = (event, target) => {
                event.preventDefault();
                event.stopPropagation();
                onOpenContextMenu?.(event.clientX, event.clientY, target);
        };

        return (
                <div className="device-tree-panel" onMouseDown={(event) => event.stopPropagation()}>
                        <ScrollArea className="device-tree-scroll parameter-scroll-area">
                                {loading ? <div className="device-tree-state">loading device tree</div> : null}
                                {!loading && error ? <div className="device-tree-state is-error">{error}</div> : null}
                                {!loading && !error && !displayedRoot ? <div className="device-tree-state">{search.test ? 'no matches' : 'no device tree nodes'}</div> : null}
                                {!loading && !error && displayedRoot ? (
                                        <DeviceTreeNode
                                                node={displayedRoot}
                                                depth={0}
                                                expandedPaths={expandedPaths}
                                                onToggle={toggle}
                                                onContextMenu={openContextMenu}
                                                forceExpand={Boolean(search.test)}
                                                pattern={search.highlight}
                                        />
                                ) : null}
                        </ScrollArea>
                        {tree?.truncated ? <div className="device-tree-state is-error">node limit reached, tree is truncated</div> : null}
                </div>
        );
}
