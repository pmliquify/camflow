import React, { useEffect, useRef, useState } from 'react';
import RuntimeLane from './RuntimeLane.jsx';
import ResetViewButton from '../../components/ResetViewButton.jsx';
import StandardContextMenu from '../../components/StandardContextMenu.jsx';

export default function NodeEditorPanel({
        viewMode,
        editorViewportRef,
        onOpenBackgroundMenu,
        onWheelCapture,
        editorZoom,
        editorPanX,
        editorPanY,
        onResetView,
        canvasWidth,
        canvasHeight,
        onPanMouseDown,
        absoluteCrossEdges,
        absoluteEdgeCurvePath,
        onDeleteEdgeByText,
        editorGraph,
        localIp,
        renameRuntime,
        renameNode,
        setNodePortVisibility,
        deleteEdgesForPort,
        selectedNodeId,
        suppressNextNodeSelectRef,
        connectNodes,
        setSelectedNodeId,
        openMenu,
        setSelectedRuntimeId,
        edgeCurvePath,
        startNodeDrag,
        startRuntimeDrag,
        startRuntimeResize,
        onStartRuntime,
        runtimeDisplayLabel,
        runtimeLogs,
        runtimeLogPanels,
        onToggleRuntimeLogPanel,
        onClearRuntimeLogs,
        runtimeBaseUrl,
        selectedMediaElement,
        onSelectMediaElement
}) {
        const [edgeDraft, setEdgeDraft] = useState(null);
        const [portMenu, setPortMenu] = useState({ open: false, direction: '', x: 0, y: 0, nodeId: '', ports: [] });
        const isEdgeDraftActive = Boolean(edgeDraft);
        const edgeDraftRef = useRef(null);
        const runtimesRef = useRef(editorGraph.runtimes);

        useEffect(() => {
                edgeDraftRef.current = edgeDraft;
        }, [edgeDraft]);

        useEffect(() => {
                runtimesRef.current = editorGraph.runtimes;
        }, [editorGraph.runtimes]);

        useEffect(() => {
                if (!isEdgeDraftActive) {
                        return undefined;
                }

                const toGraphPoint = (event) => {
                        const viewport = editorViewportRef.current;
                        const currentDraft = edgeDraftRef.current;
                        if (!viewport || !currentDraft) {
                                return { x: 0, y: 0 };
                        }
                        const rect = viewport.getBoundingClientRect();
                        return {
                                x: (event.clientX - rect.left - editorPanX) / Math.max(editorZoom, 0.0001),
                                y: (event.clientY - rect.top - editorPanY) / Math.max(editorZoom, 0.0001)
                        };
                };

                const moveHandler = (event) => {
                        const nextPoint = toGraphPoint(event);
                        setEdgeDraft((current) => (current ? { ...current, to: nextPoint } : current));
                };

                const finishHandler = (event) => {
                        const currentDraft = edgeDraftRef.current;
                        if (!currentDraft) {
                                return;
                        }
                        if (Math.hypot(currentDraft.to.x - currentDraft.from.x, currentDraft.to.y - currentDraft.from.y) < 2) {
                                return;
                        }
                        const elementAtPoint = document.elementFromPoint(event.clientX, event.clientY);
                        const inputRow = elementAtPoint instanceof Element ? elementAtPoint.closest('.node-port-row[data-port-direction="input"]') : null;
                        const nodeCard = inputRow instanceof Element ? inputRow.closest('.node-card') : null;
                        const targetNodeId = nodeCard instanceof HTMLElement ? nodeCard.dataset.nodeId || '' : '';
                        const targetPortName = inputRow instanceof HTMLElement ? inputRow.dataset.portName || '' : '';

                        if (targetNodeId && targetPortName && targetNodeId !== currentDraft.sourceNodeId) {
                                const targetNode = runtimesRef.current.flatMap((runtime) => runtime.nodes).find((node) => node.id === targetNodeId);
                                const targetPort = (targetNode?.inputs || []).find((input) => input.name === targetPortName);
                                if (targetPort && String(targetPort.type || targetPort.dataType) === currentDraft.sourceType) {
                                        void connectNodes(currentDraft.sourceNodeId, targetNodeId, currentDraft.sourcePort, targetPortName);
                                }
                        }
                        setEdgeDraft(null);
                };

                const cancelHandler = () => {
                        setEdgeDraft(null);
                };

                window.addEventListener('mousemove', moveHandler);
                window.addEventListener('mouseup', finishHandler);
                window.addEventListener('blur', cancelHandler);
                document.addEventListener('mouseleave', cancelHandler);
                return () => {
                        window.removeEventListener('mousemove', moveHandler);
                        window.removeEventListener('mouseup', finishHandler);
                        window.removeEventListener('blur', cancelHandler);
                        document.removeEventListener('mouseleave', cancelHandler);
                };
        }, [connectNodes, editorPanX, editorPanY, editorViewportRef, editorZoom, isEdgeDraftActive]);

        const edgeDraftPath = edgeDraft
                ? `M ${edgeDraft.from.x} ${edgeDraft.from.y} C ${edgeDraft.from.x + Math.max(72, Math.min(220, Math.abs(edgeDraft.to.x - edgeDraft.from.x) * 0.45))} ${edgeDraft.from.y}, ${edgeDraft.to.x - Math.max(72, Math.min(220, Math.abs(edgeDraft.to.x - edgeDraft.from.x) * 0.45))} ${edgeDraft.to.y}, ${edgeDraft.to.x} ${edgeDraft.to.y}`
                : null;

        const startEdgeDraft = (event, node, portName) => {
                if (event.button !== 0) {
                        return;
                }
                const viewport = editorViewportRef.current;
                const port = (node.outputs || []).find((candidate) => candidate.name === portName);
                if (!viewport || !port) {
                        return;
                }
                event.preventDefault();
                event.stopPropagation();
                const viewportRect = viewport.getBoundingClientRect();
                const portRect = event.currentTarget.getBoundingClientRect();
                const from = {
                        x: (portRect.right - viewportRect.left - editorPanX) / Math.max(editorZoom, 0.0001),
                        y: (portRect.top + portRect.height * 0.5 - viewportRect.top - editorPanY) / Math.max(editorZoom, 0.0001)
                };
                setEdgeDraft({ sourceNodeId: node.id, sourcePort: port.name, sourceType: String(port.type || port.dataType), from, to: from });
        };

        const openPortMenu = (event, node, direction) => {
                event.stopPropagation();
                setEdgeDraft(null);
                const availablePorts = direction === 'input' ? node.inputs || [] : node.outputs || [];
                const visibleNames = new Set(direction === 'input' ? node.visibleInputs || [] : node.visibleOutputs || []);
                const hiddenPorts = availablePorts.filter((port) => !visibleNames.has(port.name));
                if (hiddenPorts.length === 0) {
                        setPortMenu((current) => ({ ...current, open: false }));
                        return;
                }
                setPortMenu({
                        open: true,
                        direction,
                        x: event.clientX,
                        y: event.clientY,
                        nodeId: node.id,
                        ports: hiddenPorts
                });
        };

        const updatePortVisibility = (node, direction, update) => {
                const key = direction === 'input' ? 'inputs' : 'outputs';
                setNodePortVisibility((current) => ({
                        ...current,
                        [node.id]: {
                                inputs: node.visibleInputs || [],
                                outputs: node.visibleOutputs || [],
                                [key]: update(key === 'inputs' ? node.visibleInputs || [] : node.visibleOutputs || [])
                        }
                }));
        };

        const showPort = (port) => {
                const node = runtimesRef.current.flatMap((runtime) => runtime.nodes).find((candidate) => candidate.id === portMenu.nodeId);
                if (node) {
                        updatePortVisibility(node, portMenu.direction, (visible) => [...new Set([...visible, port.name])]);
                }
                setPortMenu((current) => ({ ...current, open: false }));
        };

        return (
                <section className={`panel editor-panel ${viewMode === 'editor' ? 'foreground' : 'background'}`}>
                        <div
                                ref={editorViewportRef}
                                className="runtime-stack"
                                onContextMenu={onOpenBackgroundMenu}
                                onWheelCapture={onWheelCapture}
                        >
                                <div className="editor-tools">
                                        <ResetViewButton onClick={onResetView} className="editor-reset-view-button" />
                                </div>
                                <div
                                        className="runtime-stack-inner"
                                        style={{
                                                width: `${canvasWidth}px`,
                                                height: `${canvasHeight}px`,
                                                transform: `translate(${editorPanX}px, ${editorPanY}px) scale(${editorZoom})`,
                                                transformOrigin: '0 0'
                                        }}
                                        onMouseDown={onPanMouseDown}
                                >
                                        <svg className="cross-edge-layer" viewBox={`0 0 ${canvasWidth} ${canvasHeight}`} preserveAspectRatio="none">
                                                <defs>
                                                        <marker id="cross-edge-arrow" markerWidth="10" markerHeight="11" refX="9" refY="5.5" orient="auto" markerUnits="userSpaceOnUse">
                                                                <path d="M 1.25 2.25 L 8.75 5.5 L 1.25 8.75 Z" fill="context-stroke" stroke="context-stroke" strokeWidth="2.5" strokeLinejoin="round" />
                                                        </marker>
                                                </defs>
                                                {absoluteCrossEdges.map((edge) => (
                                                        <g key={edge.id}>
                                                                <path
                                                                        className="cross-edge-hit-path"
                                                                        d={absoluteEdgeCurvePath(edge.from, edge.to, edge)}
                                                                        onContextMenu={(event) => {
                                                                                event.preventDefault();
                                                                                event.stopPropagation();
                                                                                const edgeText = `${edge.fromNode}.${edge.fromPort || 'image'} -> ${edge.toNode}.${edge.toPort || 'image'}`;
                                                                                void onDeleteEdgeByText(edgeText);
                                                                        }}
                                                                />
                                                                <path className="cross-edge-visual" d={absoluteEdgeCurvePath(edge.from, edge.to, edge)} markerEnd="url(#cross-edge-arrow)" />
                                                        </g>
                                                ))}
                                        </svg>
                                        {edgeDraftPath ? (
                                                <svg className="edge-draft-layer" viewBox={`0 0 ${canvasWidth} ${canvasHeight}`} preserveAspectRatio="none">
                                                        <defs>
                                                                <marker id="draft-edge-arrow" markerWidth="10" markerHeight="11" refX="9" refY="5.5" orient="auto" markerUnits="userSpaceOnUse">
                                                                        <path d="M 1.5 2.5 L 8.5 5.5 L 1.5 8.5 Z" fill="context-stroke" stroke="context-stroke" strokeWidth="3" strokeLinejoin="round" />
                                                                </marker>
                                                        </defs>
                                                        <path d={edgeDraftPath} markerEnd="url(#draft-edge-arrow)" />
                                                </svg>
                                        ) : null}
                                        {editorGraph.runtimes.map((runtime) => (
                                                <RuntimeLane
                                                        key={runtime.id}
                                                        runtime={{
                                                                ...runtime,
                                                                displayName: runtimeDisplayLabel(runtime, localIp),
                                                                onRename: (nextName) => renameRuntime(runtime.id, nextName),
                                                                onRenameNode: (nodeId, nextName) => renameNode(nodeId, nextName)
                                                        }}
                                                        logEntries={runtimeLogs?.[runtime.id] || []}
                                                        logOpen={runtimeLogPanels?.[runtime.id] !== false}
                                                        onToggleLogOpen={onToggleRuntimeLogPanel}
                                                        selectedNodeId={selectedNodeId}
                                                        onSelectNode={(nodeId) => {
                                                                if (suppressNextNodeSelectRef?.current) {
                                                                        suppressNextNodeSelectRef.current = false;
                                                                        return;
                                                                }
                                                                setSelectedNodeId(nodeId);
                                                        }}
                                                        onLaneContextMenu={(event, runtimeId) => openMenu(event, 'runtime', runtimeId)}
                                                        onNodeContextMenu={(event, runtimeId, nodeId) => {
                                                                setSelectedRuntimeId(runtimeId);
                                                                openMenu(event, 'node', runtimeId, nodeId);
                                                        }}
                                                        onEdgePath={edgeCurvePath}
                                                        onDeleteEdge={(edge) => {
                                                                const edgeText = `${edge.fromNode}.${edge.fromPort || 'image'} -> ${edge.toNode}.${edge.toPort || 'image'}`;
                                                                void onDeleteEdgeByText(edgeText);
                                                        }}
                                                        onNodeDragStart={(event, runtimeId, nodeId) => {
                                                                if (edgeDraftRef.current) {
                                                                        return;
                                                                }
                                                                startNodeDrag(event, runtimeId, nodeId);
                                                        }}
                                                        onNodePortAreaClick={(event, runtimeId, node, direction) => {
                                                                setSelectedRuntimeId(runtimeId);
                                                                setSelectedNodeId(node.id);
                                                                openPortMenu(event, node, direction);
                                                        }}
                                                        onNodePortMouseDown={(event, runtimeId, node, portName) => {
                                                                setSelectedRuntimeId(runtimeId);
                                                                setSelectedNodeId(node.id);
                                                                startEdgeDraft(event, node, portName);
                                                        }}
                                                        onHideNodePort={async (node, direction, portName) => {
                                                                if (await deleteEdgesForPort(node.id, direction, portName)) {
                                                                        updatePortVisibility(node, direction, (visible) => visible.filter((name) => name !== portName));
                                                                }
                                                        }}
                                                        onRuntimeDragStart={startRuntimeDrag}
                                                        onRuntimeResizeStart={startRuntimeResize}
                                                        onStartRuntime={onStartRuntime}
                                                        onClearRuntimeLogs={onClearRuntimeLogs}
                                                        runtimeBaseUrl={runtime.id === 'local' ? '' : runtimeBaseUrl(runtime.ip)}
                                                        selectedMediaElement={selectedMediaElement}
                                                        onSelectMediaElement={onSelectMediaElement}
                                                />
                                        ))}
                                </div>
                        </div>
                        <StandardContextMenu
                                open={portMenu.open}
                                x={portMenu.x}
                                y={portMenu.y}
                                onClose={() => {
                                        setPortMenu((current) => ({ ...current, open: false }));
                                }}
                                items={portMenu.ports.map((port) => ({
                                        id: port.name,
                                        label: `${port.name} · ${port.type || port.dataType}`,
                                        onSelect: () => showPort(port)
                                }))}
                                emptyLabel="all ports visible"
                        />
                </section>
        );
}
