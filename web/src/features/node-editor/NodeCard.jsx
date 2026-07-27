import React from 'react';
import UiButton from '../../components/UiButton.jsx';

export default function NodeCard({ node, selected, onSelect, onRename, onDragStart, onContextMenu, onEdgeHandleMouseDown }) {
        return (
                <div
                        className={`node-card ${selected ? 'selected' : ''}`}
                        data-node-id={node.id}
                        style={{ left: node.x, top: node.y }}
                        onClick={() => onSelect(node.id)}
                        onMouseDown={(event) => onDragStart(event, node.id)}
                        onContextMenu={(event) => {
                                event.preventDefault();
                                event.stopPropagation();
                                onContextMenu(event, node.id);
                        }}
                >
                        <div className="node-title" onDoubleClick={onRename}>{node.name || node.id}</div>
                        <div className="node-type">{node.type}</div>
                        {!node.live ? <div className="node-badge">editor</div> : null}
                        <UiButton
                                type="button"
                                className="node-edge-handle"
                                iconOnly={true}
                                aria-label={`connect from ${node.name || node.id}`}
                                onClick={(event) => event.stopPropagation()}
                                onMouseDown={(event) => {
                                        event.preventDefault();
                                        event.stopPropagation();
                                        onEdgeHandleMouseDown(event, node.id);
                                }}
                        />
                </div>
        );
}
