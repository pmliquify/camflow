import React from 'react';
import InlineNameEditor from '../../components/InlineNameEditor.jsx';

function PortSide({ direction, ports, disabled, onContextMenu, onPortMouseDown }) {
        return (
                <div
                        className={`node-port-side node-port-side-${direction}${disabled ? ' disabled' : ''}`}
                        data-node-side={direction}
                        onContextMenu={disabled ? undefined : onContextMenu}
                        onMouseDown={(event) => event.stopPropagation()}
                >
                        {(ports.length ? ports : ['']).map((portName, index) => (
                                <span
                                        className={`node-port-row${portName ? '' : ' empty'}`}
                                        data-port-name={portName}
                                        data-port-direction={direction}
                                        key={portName || `empty-${index}`}
                                        onMouseDown={portName && direction === 'output' ? (event) => onPortMouseDown(event, portName) : undefined}
                                >
                                        {portName || 'no port'}
                                </span>
                        ))}
                </div>
        );
}

export default function NodeCard({ node, selected, onSelect, onRename, onDragStart, onContextMenu, onPortContextMenu, onPortMouseDown }) {
        return (
                <div
                        className={`node-card ${selected ? 'selected' : ''}`}
                        data-node-id={node.id}
                        style={{ left: node.x, top: node.y, height: node.height }}
                        onClick={() => onSelect(node.id)}
                        onMouseDown={(event) => onDragStart(event, node.id)}
                        onContextMenu={(event) => onContextMenu(event, node)}
                >
                        <div className="node-title">
                                <InlineNameEditor
                                        value={node.name || node.id}
                                        onCommit={onRename}
                                        className="node-title-text"
                                        inputClassName="inline-name-input node-name-editor"
                                        ariaLabel="node name"
                                />
                        </div>
                        <div className="node-port-body">
                                <PortSide
                                        direction="input"
                                        ports={node.visibleInputs || []}
                                        disabled={!node.inputs?.length}
                                        onContextMenu={(event) => onPortContextMenu(event, node, 'input')}
                                />
                                <PortSide
                                        direction="output"
                                        ports={node.visibleOutputs || []}
                                        disabled={!node.outputs?.length}
                                        onContextMenu={(event) => onPortContextMenu(event, node, 'output')}
                                        onPortMouseDown={(event, portName) => onPortMouseDown(event, node, portName)}
                                />
                        </div>
                </div>
        );
}
