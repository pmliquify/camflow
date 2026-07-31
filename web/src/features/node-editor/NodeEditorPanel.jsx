import React, { useEffect, useRef, useState } from 'react';
import RuntimeLane from './RuntimeLane.jsx';
import ResetViewButton from '../../components/ResetViewButton.jsx';

const NODE_WIDTH = 152;
const NODE_HEIGHT = 62;
const RUNTIME_HEADER_HEIGHT = 25;

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
        runtimeIpEditMode,
        runtimeIpDrafts,
        beginRuntimeIpEdit,
        setRuntimeIpDrafts,
        renameRuntime,
        renameNode,
        selectedNodeId,
        suppressNextNodeSelectRef,
        pendingEdgeSourceId,
        connectNodes,
        setSelectedNodeId,
        setPendingEdgeSourceId,
        openMenu,
        setSelectedRuntimeId,
        edgeCurvePath,
        startNodeDrag,
        startRuntimeDrag,
        startRuntimeResize,
        onStartRuntime,
        commitRuntimeIpEdit,
        runtimeDisplayLabel,
        runtimeLogs,
        runtimeLogPanels,
        onToggleRuntimeLogPanel,
        onClearRuntimeLogs
}) {
        const [edgeDraft, setEdgeDraft] = useState(null);
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
                        let targetNodeId = '';

                        const elementAtPoint = document.elementFromPoint(event.clientX, event.clientY);
                        const nodeCard = elementAtPoint instanceof Element ? elementAtPoint.closest('.node-card') : null;
                        if (nodeCard instanceof HTMLElement) {
                                targetNodeId = nodeCard.dataset.nodeId || '';
                        }

                        if (!targetNodeId) {
                                const dropPoint = toGraphPoint(event);
                                runtimesRef.current.some((runtime) => runtime.nodes.some((node) => {
                                        const left = runtime.rect.x + node.x;
                                        const top = runtime.rect.y + RUNTIME_HEADER_HEIGHT + node.y;
                                        const hit = dropPoint.x >= left && dropPoint.x <= left + NODE_WIDTH && dropPoint.y >= top && dropPoint.y <= top + NODE_HEIGHT;
                                        if (hit) {
                                                targetNodeId = node.id;
                                        }
                                        return hit;
                                }));
                        }

                        if (targetNodeId && targetNodeId !== currentDraft.sourceNodeId) {
                                void connectNodes(currentDraft.sourceNodeId, targetNodeId);
                        }
                        setEdgeDraft(null);
                };

                const cancelHandler = () => {
                        setEdgeDraft(null);
                };

                window.addEventListener('mousemove', moveHandler);
                window.addEventListener('mouseup', finishHandler);
                window.addEventListener('pointerup', finishHandler);
                window.addEventListener('blur', cancelHandler);
                document.addEventListener('mouseleave', cancelHandler);
                return () => {
                        window.removeEventListener('mousemove', moveHandler);
                        window.removeEventListener('mouseup', finishHandler);
                        window.removeEventListener('pointerup', finishHandler);
                        window.removeEventListener('blur', cancelHandler);
                        document.removeEventListener('mouseleave', cancelHandler);
                };
        }, [connectNodes, editorPanX, editorPanY, editorViewportRef, editorZoom, isEdgeDraftActive]);

        const edgeDraftPath = edgeDraft
                ? `M ${edgeDraft.from.x} ${edgeDraft.from.y} C ${edgeDraft.from.x + Math.max(72, Math.min(220, Math.abs(edgeDraft.to.x - edgeDraft.from.x) * 0.45))} ${edgeDraft.from.y}, ${edgeDraft.to.x - Math.max(72, Math.min(220, Math.abs(edgeDraft.to.x - edgeDraft.from.x) * 0.45))} ${edgeDraft.to.y}, ${edgeDraft.to.x} ${edgeDraft.to.y}`
                : null;

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
                                                                        d={absoluteEdgeCurvePath(edge.from, edge.to)}
                                                                        onContextMenu={(event) => {
                                                                                event.preventDefault();
                                                                                event.stopPropagation();
                                                                                const edgeText = `${edge.fromNode}.${edge.fromPort || 'image'} -> ${edge.toNode}.${edge.toPort || 'image'}`;
                                                                                void onDeleteEdgeByText(edgeText);
                                                                        }}
                                                                />
                                                                <path className="cross-edge-visual" d={absoluteEdgeCurvePath(edge.from, edge.to)} markerEnd="url(#cross-edge-arrow)" />
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
                                                                editingIp: Boolean(runtimeIpEditMode[runtime.id]),
                                                                ipDraft: runtimeIpDrafts[runtime.id] ?? runtime.ip,
                                                                onEditIp: () => beginRuntimeIpEdit(runtime),
                                                                onIpDraft: (value) => setRuntimeIpDrafts((current) => ({ ...current, [runtime.id]: value })),
                                                                onRename: () => renameRuntime(runtime.id),
                                                                onRenameNode: (nodeId) => renameNode(nodeId)
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
                                                                if (pendingEdgeSourceId && pendingEdgeSourceId !== nodeId) {
                                                                        void connectNodes(pendingEdgeSourceId, nodeId);
                                                                        return;
                                                                }
                                                                setSelectedNodeId(nodeId);
                                                                if (pendingEdgeSourceId === nodeId) {
                                                                        setPendingEdgeSourceId('');
                                                                }
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
                                                        onNodeEdgeStart={(event, runtimeId, nodeId) => {
                                                                if (event.button !== 0) {
                                                                        return;
                                                                }
                                                                const viewport = editorViewportRef.current;
                                                                if (!viewport) {
                                                                        return;
                                                                }
                                                                const rect = viewport.getBoundingClientRect();
                                                                const handleRect = event.currentTarget.getBoundingClientRect();
                                                                const from = {
                                                                        x: (handleRect.left + handleRect.width * 0.5 - rect.left - editorPanX) / Math.max(editorZoom, 0.0001),
                                                                        y: (handleRect.top + handleRect.height * 0.5 - rect.top - editorPanY) / Math.max(editorZoom, 0.0001)
                                                                };
                                                                const to = {
                                                                        x: (event.clientX - rect.left - editorPanX) / Math.max(editorZoom, 0.0001),
                                                                        y: (event.clientY - rect.top - editorPanY) / Math.max(editorZoom, 0.0001)
                                                                };
                                                                setSelectedRuntimeId(runtimeId);
                                                                setEdgeDraft({ sourceNodeId: nodeId, from, to });
                                                        }}
                                                        onRuntimeDragStart={startRuntimeDrag}
                                                        onRuntimeResizeStart={startRuntimeResize}
                                                        onStartRuntime={onStartRuntime}
                                                        onIpCommit={async (runtimeId, ok) => {
                                                                const target = editorGraph.runtimes.find((item) => item.id === runtimeId);
                                                                await commitRuntimeIpEdit(target, ok);
                                                        }}
                                                        onClearRuntimeLogs={onClearRuntimeLogs}
                                                />
                                        ))}
                                </div>
                        </div>
                </section>
        );
}
