import React from 'react';

export default function EdgeLayer({ runtime, onEdgePath, onDeleteEdge }) {
        const nodesById = new Map(runtime.nodes.map((node) => [node.id, node]));
        const markerId = `edge-arrow-${String(runtime.id || 'runtime').replace(/[^a-zA-Z0-9_-]/g, '-')}`;
        return (
                <svg className="edge-layer">
                        <defs>
                                <marker id={markerId} markerWidth="10" markerHeight="11" refX="9" refY="5.5" orient="auto" markerUnits="userSpaceOnUse">
                                        <path d="M 1.5 2.5 L 8.5 5.5 L 1.5 8.5 Z" fill="context-stroke" stroke="context-stroke" strokeWidth="3" strokeLinejoin="round" />
                                </marker>
                        </defs>
                        {runtime.edges.map((edge) => {
                                const fromNode = nodesById.get(edge.fromNode);
                                const toNode = nodesById.get(edge.toNode);
                                if (!fromNode || !toNode) {
                                        return null;
                                }
                                const pathD = onEdgePath(fromNode, toNode, edge);
                                return (
                                        <g key={edge.id}>
                                                <path
                                                        className="edge-hit-path"
                                                        d={pathD}
                                                        onContextMenu={(event) => {
                                                                event.preventDefault();
                                                                event.stopPropagation();
                                                                if (onDeleteEdge) {
                                                                        onDeleteEdge(edge);
                                                                }
                                                        }}
                                                />
                                                <path className="edge-visual" d={pathD} markerEnd={`url(#${markerId})`} />
                                        </g>
                                );
                        })}
                </svg>
        );
}
