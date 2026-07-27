import React from 'react';

export default function EdgeLayer({ runtime, onEdgePath, onDeleteEdge }) {
        const nodesById = new Map(runtime.nodes.map((node) => [node.id, node]));
        const markerId = `edge-arrow-${String(runtime.id || 'runtime').replace(/[^a-zA-Z0-9_-]/g, '-')}`;
        return (
                <svg className="edge-layer">
                        <defs>
                                <marker id={markerId} markerWidth="6" markerHeight="5" refX="5.4" refY="2.5" orient="auto" markerUnits="userSpaceOnUse">
                                        <path d="M 0 0 L 6 2.5 L 0 5 z" fill="#3ae0a7" opacity="0.8" />
                                </marker>
                        </defs>
                        {runtime.edges.map((edge) => {
                                const fromNode = nodesById.get(edge.fromNode);
                                const toNode = nodesById.get(edge.toNode);
                                if (!fromNode || !toNode) {
                                        return null;
                                }
                                const pathD = onEdgePath(fromNode, toNode);
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
