import React, { useId, useMemo } from 'react';
import {
        hasMediaFlag,
        MEDIA_LINK_FLAG_DYNAMIC,
        MEDIA_LINK_FLAG_ENABLED,
        MEDIA_LINK_FLAG_IMMUTABLE,
        MEDIA_PAD_FLAG_SOURCE
} from '../../utils/mediaFlags.js';

const ENTITY_WIDTH = 150;
const ENTITY_BORDER = 1;
const HEADER_HEIGHT = 42;
const PAD_HEIGHT = 24;
const COLUMN_GAP = 64;
const ROW_GAP = 30;
const MARGIN = 28;

function assignLayers(entities, links) {
        const entityIds = new Set(entities.map((entity) => entity.id));
        const outgoing = new Map(entities.map((entity) => [entity.id, []]));
        const indegree = new Map(entities.map((entity) => [entity.id, 0]));
        links.forEach((link) => {
                if (!entityIds.has(link.sourceEntityId) || !entityIds.has(link.sinkEntityId) || link.sourceEntityId === link.sinkEntityId) return;
                outgoing.get(link.sourceEntityId).push(link.sinkEntityId);
                indegree.set(link.sinkEntityId, indegree.get(link.sinkEntityId) + 1);
        });

        const layerById = new Map(entities.map((entity) => [entity.id, 0]));
        const queue = entities.filter((entity) => indegree.get(entity.id) === 0).map((entity) => entity.id);
        const processed = new Set();
        while (queue.length > 0) {
                const entityId = queue.shift();
                if (processed.has(entityId)) continue;
                processed.add(entityId);
                for (const sinkId of outgoing.get(entityId) || []) {
                        layerById.set(sinkId, Math.max(layerById.get(sinkId), layerById.get(entityId) + 1));
                        indegree.set(sinkId, indegree.get(sinkId) - 1);
                        if (indegree.get(sinkId) === 0) queue.push(sinkId);
                }
        }

        // Keep cyclic components finite and deterministic while still placing them after known predecessors.
        const remaining = entities.filter((entity) => !processed.has(entity.id));
        remaining.forEach((entity, index) => layerById.set(entity.id, Math.max(layerById.get(entity.id), index)));
        return layerById;
}

function orderPositions(layers) {
        const positions = new Map();
        layers.forEach((layer) => layer.forEach((entity, index) => positions.set(entity.id, index)));
        return positions;
}

function endpointPosition(entityId, padId, positions, entityById) {
        const entity = entityById.get(entityId);
        const pads = entity?.pads || [];
        const padIndex = Math.max(0, pads.findIndex((pad) => pad.id === padId));
        return (positions.get(entityId) || 0) + (padIndex + 1) / (pads.length + 1);
}

function crossingCount(layers, links, layerById) {
        const positions = orderPositions(layers);
        const entityById = new Map(layers.flat().map((entity) => [entity.id, entity]));
        const groupedLinks = new Map();
        links.forEach((link) => {
                const sourceLayer = layerById.get(link.sourceEntityId);
                const sinkLayer = layerById.get(link.sinkEntityId);
                if (sourceLayer === undefined || sinkLayer === undefined || sourceLayer === sinkLayer) return;
                const lowLayer = Math.min(sourceLayer, sinkLayer);
                const highLayer = Math.max(sourceLayer, sinkLayer);
                const key = `${lowLayer}:${highLayer}`;
                const endpoint = sourceLayer < sinkLayer
                        ? {
                                low: endpointPosition(link.sourceEntityId, link.sourcePadId, positions, entityById),
                                high: endpointPosition(link.sinkEntityId, link.sinkPadId, positions, entityById)
                        }
                        : {
                                low: endpointPosition(link.sinkEntityId, link.sinkPadId, positions, entityById),
                                high: endpointPosition(link.sourceEntityId, link.sourcePadId, positions, entityById)
                        };
                if (!groupedLinks.has(key)) groupedLinks.set(key, []);
                groupedLinks.get(key).push(endpoint);
        });

        let crossings = 0;
        groupedLinks.forEach((group) => {
                for (let left = 0; left < group.length; left += 1) {
                        for (let right = left + 1; right < group.length; right += 1) {
                                if ((group[left].low - group[right].low) * (group[left].high - group[right].high) < 0) crossings += 1;
                        }
                }
        });
        return crossings;
}

function barycentricSort(layers, links, layerById, forward) {
        const positions = orderPositions(layers);
        const layerIndexes = forward
                ? Array.from({ length: layers.length - 1 }, (_, index) => index + 1)
                : Array.from({ length: layers.length - 1 }, (_, index) => layers.length - index - 2);
        layerIndexes.forEach((layerIndex) => {
                const scored = layers[layerIndex].map((entity, stableIndex) => {
                        const neighbors = links.flatMap((link) => {
                                if (forward && link.sinkEntityId === entity.id && layerById.get(link.sourceEntityId) < layerIndex) return [positions.get(link.sourceEntityId)];
                                if (!forward && link.sourceEntityId === entity.id && layerById.get(link.sinkEntityId) > layerIndex) return [positions.get(link.sinkEntityId)];
                                return [];
                        }).filter(Number.isFinite);
                        const score = neighbors.length ? neighbors.reduce((sum, position) => sum + position, 0) / neighbors.length : stableIndex;
                        return { entity, score, stableIndex };
                });
                scored.sort((left, right) => left.score - right.score || left.stableIndex - right.stableIndex || left.entity.id - right.entity.id);
                layers[layerIndex] = scored.map((item) => item.entity);
        });
}

function optimizeLayerOrder(entities, links, layerById) {
        const layerCount = Math.max(0, ...layerById.values()) + 1;
        let layers = Array.from({ length: layerCount }, () => []);
        entities.forEach((entity) => layers[layerById.get(entity.id) || 0].push(entity));
        layers.forEach((layer) => layer.sort((left, right) => left.id - right.id || String(left.name).localeCompare(String(right.name))));

        let best = layers.map((layer) => [...layer]);
        let bestCrossings = crossingCount(best, links, layerById);
        for (let sweep = 0; sweep < 8; sweep += 1) {
                barycentricSort(layers, links, layerById, sweep % 2 === 0);
                const crossings = crossingCount(layers, links, layerById);
                if (crossings < bestCrossings) {
                        bestCrossings = crossings;
                        best = layers.map((layer) => [...layer]);
                }
        }

        layers = best.map((layer) => [...layer]);
        let improved = true;
        while (improved) {
                improved = false;
                for (const layer of layers) {
                        for (let index = 0; index + 1 < layer.length; index += 1) {
                                [layer[index], layer[index + 1]] = [layer[index + 1], layer[index]];
                                const crossings = crossingCount(layers, links, layerById);
                                if (crossings < bestCrossings) {
                                        bestCrossings = crossings;
                                        improved = true;
                                } else {
                                        [layer[index], layer[index + 1]] = [layer[index + 1], layer[index]];
                                }
                        }
                }
        }
        return layers;
}

function layoutGraph(graph) {
        const entities = Array.isArray(graph?.entities) ? graph.entities : [];
        const entityIds = new Set(entities.map((entity) => entity.id));
        const links = (Array.isArray(graph?.links) ? graph.links : []).filter((link) => entityIds.has(link.sourceEntityId) && entityIds.has(link.sinkEntityId));
        const layerById = assignLayers(entities, links.filter((link) => hasMediaFlag(link.flags, MEDIA_LINK_FLAG_ENABLED)));
        const layers = optimizeLayerOrder(entities, links, layerById);
        const layerHeights = layers.map((layer) => layer.reduce((sum, entity, index) => {
                const entityHeight = ENTITY_BORDER * 2 + HEADER_HEIGHT + Math.max(1, entity.pads?.length || 0) * PAD_HEIGHT;
                return sum + entityHeight + (index > 0 ? ROW_GAP : 0);
        }, 0));
        const contentHeight = Math.max(0, ...layerHeights);
        const positioned = layers.flatMap((layer, layerIndex) => {
                let y = MARGIN + (contentHeight - layerHeights[layerIndex]) / 2;
                return layer.map((entity) => {
                        const height = ENTITY_BORDER * 2 + HEADER_HEIGHT + Math.max(1, entity.pads?.length || 0) * PAD_HEIGHT;
                        const positionedEntity = { ...entity, x: MARGIN + layerIndex * (ENTITY_WIDTH + COLUMN_GAP), y, width: ENTITY_WIDTH, height };
                        y += height + ROW_GAP;
                        return positionedEntity;
                });
        });
        const width = Math.max(480, ...positioned.map((entity) => entity.x + entity.width + MARGIN));
        const height = Math.max(220, ...positioned.map((entity) => entity.y + entity.height + MARGIN));
        return { entities: positioned, width, height };
}

function padAnchor(entity, pad) {
        const padIndex = Math.max(0, (entity.pads || []).findIndex((candidate) => candidate.id === pad.id));
        return {
                x: hasMediaFlag(pad.flags, MEDIA_PAD_FLAG_SOURCE) ? entity.x + entity.width : entity.x,
                y: entity.y + ENTITY_BORDER + HEADER_HEIGHT + padIndex * PAD_HEIGHT + PAD_HEIGHT / 2
        };
}

function linkPath(source, sink) {
        const control = Math.max(44, Math.abs(sink.x - source.x) * 0.42);
        return `M ${source.x} ${source.y} C ${source.x + control} ${source.y}, ${sink.x - control} ${sink.y}, ${sink.x} ${sink.y}`;
}

function linkClass(link) {
        return [
                hasMediaFlag(link.flags, MEDIA_LINK_FLAG_ENABLED) ? 'media-link-enabled' : 'media-link-disabled',
                hasMediaFlag(link.flags, MEDIA_LINK_FLAG_IMMUTABLE) ? 'media-link-immutable' : '',
                hasMediaFlag(link.flags, MEDIA_LINK_FLAG_DYNAMIC) ? 'media-link-dynamic' : ''
        ].filter(Boolean).join(' ');
}

function connectedGraph(graph, connectedOnly) {
        if (!connectedOnly || !graph) return graph;
        const links = (graph.links || []).filter((link) => hasMediaFlag(link.flags, MEDIA_LINK_FLAG_ENABLED));
        const entityIds = new Set(links.flatMap((link) => [link.sourceEntityId, link.sinkEntityId]));
        const padIds = new Set(links.flatMap((link) => [link.sourcePadId, link.sinkPadId]));
        return {
                ...graph,
                links,
                entities: (graph.entities || [])
                        .filter((entity) => entityIds.has(entity.id))
                        .map((entity) => ({ ...entity, pads: (entity.pads || []).filter((pad) => padIds.has(pad.id)) }))
        };
}

export default function MediaGraphView({ graph, loading, error, connectedOnly, selectedElement, onSelectElement, zoom = 1, panX = 0, panY = 0 }) {
        const markerId = `media-link-arrow-${useId().replace(/:/g, '')}`;
        const enabledMarkerId = `${markerId}-enabled`;
        const disabledMarkerId = `${markerId}-disabled`;
        const selectedEnabledMarkerId = `${markerId}-selected-enabled`;
        const selectedDisabledMarkerId = `${markerId}-selected-disabled`;
        const displayedGraph = useMemo(() => connectedGraph(graph, connectedOnly), [connectedOnly, graph]);
        const layout = useMemo(() => layoutGraph(displayedGraph), [displayedGraph]);
        const entitiesById = useMemo(() => new Map(layout.entities.map((entity) => [entity.id, entity])), [layout.entities]);

        if (loading) return <div className="media-graph-state">loading media graph</div>;
        if (error) return <div className="media-graph-state is-error">{error}</div>;
        if (connectedOnly && graph && layout.entities.length === 0) return <div className="media-graph-state">no connected links</div>;
        if (!graph || layout.entities.length === 0) return <div className="media-graph-state">no media entities</div>;

        const renderedLinks = displayedGraph.links.flatMap((link) => {
                const sourceEntity = entitiesById.get(link.sourceEntityId);
                const sinkEntity = entitiesById.get(link.sinkEntityId);
                const sourcePad = sourceEntity?.pads?.find((pad) => pad.id === link.sourcePadId);
                const sinkPad = sinkEntity?.pads?.find((pad) => pad.id === link.sinkPadId);
                if (!sourceEntity || !sinkEntity || !sourcePad || !sinkPad) return [];
                return [{
                        link,
                        path: linkPath(padAnchor(sourceEntity, sourcePad), padAnchor(sinkEntity, sinkPad)),
                        selected: selectedElement?.kind === 'link' && selectedElement?.item?.id === link.id,
                        enabled: hasMediaFlag(link.flags, MEDIA_LINK_FLAG_ENABLED),
                        immutable: hasMediaFlag(link.flags, MEDIA_LINK_FLAG_IMMUTABLE),
                        dynamic: hasMediaFlag(link.flags, MEDIA_LINK_FLAG_DYNAMIC)
                }];
        });

        return (
                <div className="media-graph-scroll" onMouseDown={(event) => event.stopPropagation()}>
                        <div
                                className="media-graph-canvas"
                                style={{
                                        width: layout.width,
                                        height: layout.height,
                                        transform: `translate(${panX}px, ${panY}px) scale(${zoom})`
                                }}
                        >
                                <svg className="media-link-layer" width={layout.width} height={layout.height}>
                                        <defs>
                                                <marker id={enabledMarkerId} markerWidth="10" markerHeight="11" refX="9" refY="5.5" orient="auto" markerUnits="userSpaceOnUse">
                                                        <path d="M 1.5 2.5 L 8.5 5.5 L 1.5 8.5 Z" fill="#39dce9" stroke="#39dce9" strokeWidth="3" strokeLinejoin="round" />
                                                </marker>
                                                <marker id={disabledMarkerId} markerWidth="10" markerHeight="11" refX="9" refY="5.5" orient="auto" markerUnits="userSpaceOnUse">
                                                        <path d="M 1.5 2.5 L 8.5 5.5 L 1.5 8.5" fill="none" stroke="#426c70" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round" />
                                                </marker>
                                                <marker id={selectedEnabledMarkerId} markerWidth="10" markerHeight="11" refX="9" refY="5.5" orient="auto" markerUnits="userSpaceOnUse" overflow="visible">
                                                        <path className="media-link-selected-arrowhead" d="M 1.5 2.5 L 8.5 5.5 L 1.5 8.5 Z" fill="#d5fdff" stroke="#d5fdff" strokeWidth="3" strokeLinejoin="round" />
                                                </marker>
                                                <marker id={selectedDisabledMarkerId} markerWidth="10" markerHeight="11" refX="9" refY="5.5" orient="auto" markerUnits="userSpaceOnUse" overflow="visible">
                                                        <path className="media-link-selected-arrowhead" d="M 1.5 2.5 L 8.5 5.5 L 1.5 8.5" fill="none" stroke="#d5fdff" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round" />
                                                </marker>
                                        </defs>
                                        {renderedLinks.map(({ link, path, selected, immutable, dynamic }) => {
                                                return (
                                                        <g key={link.id} className={`${linkClass(link)}${selected ? ' selected' : ''}`}>
                                                                {immutable ? (
                                                                        <>
                                                                                <path className="media-link-immutable-double" d={path} />
                                                                                <path className="media-link-immutable-gap" d={path} />
                                                                        </>
                                                                ) : (
                                                                        <path className="media-link-core" d={path} />
                                                                )}
                                                                {dynamic ? <path className="media-link-dynamic-dots" d={path} /> : null}
                                                                <path className="media-link-hit" d={path} onClick={(event) => { event.stopPropagation(); onSelectElement({ kind: 'link', item: link, graph }); }} />
                                                        </g>
                                                );
                                        })}
                                        {renderedLinks.map(({ link, path, enabled, selected }) => (
                                                <path
                                                        key={`arrow:${link.id}`}
                                                        className="media-link-arrow"
                                                        d={path}
                                                        markerEnd={`url(#${selected
                                                                ? (enabled ? selectedEnabledMarkerId : selectedDisabledMarkerId)
                                                                : (enabled ? enabledMarkerId : disabledMarkerId)})`}
                                                />
                                        ))}
                                </svg>
                                {layout.entities.map((entity) => {
                                        const selected = selectedElement?.kind === 'entity' && selectedElement?.item?.id === entity.id;
                                        return (
                                                <section
                                                        key={entity.id}
                                                        className={`media-entity${selected ? ' selected' : ''}`}
                                                        style={{ left: entity.x, top: entity.y, width: entity.width, height: entity.height }}
                                                        onClick={(event) => { event.stopPropagation(); onSelectElement({ kind: 'entity', item: entity, graph }); }}
                                                >
                                                        <header>
                                                                <strong title={`${entity.id}: ${entity.name}`}>{entity.id}: {entity.name}</strong>
                                                                <span title={entity.devnode || 'no devnode'}>{entity.devnode || entity.function}</span>
                                                        </header>
                                                        <div className="media-pad-list">
                                                                {(entity.pads || []).map((pad) => {
                                                                        const format = pad.pixelFormat && pad.width && pad.height ? `${pad.pixelFormat}/${pad.width}x${pad.height}` : 'format unavailable';
                                                                        const padSelected = selectedElement?.kind === 'pad' && selectedElement?.item?.id === pad.id;
                                                                        const direction = hasMediaFlag(pad.flags, MEDIA_PAD_FLAG_SOURCE) ? 'source' : 'sink';
                                                                        return (
                                                                                <button
                                                                                        key={pad.id}
                                                                                        type="button"
                                                                                        className={`media-pad media-pad-${direction}${padSelected ? ' selected' : ''}`}
                                                                                        onClick={(event) => { event.stopPropagation(); onSelectElement({ kind: 'pad', item: pad, entity, graph }); }}
                                                                                        title={`pad ${pad.index}: ${format}`}
                                                                                >
                                                                                        <span className="media-pad-index">{direction === 'source' ? `:${pad.index}` : `${pad.index}:`}</span>
                                                                                        <span className="media-pad-format">{format}</span>
                                                                                </button>
                                                                        );
                                                                })}
                                                        </div>
                                                </section>
                                        );
                                })}
                        </div>
                </div>
        );
}
