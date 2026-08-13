import React, { useMemo } from 'react';
import StandardContextMenu from '../../components/StandardContextMenu.jsx';

const GROUPS = ['sources', 'processors', 'sinks'];

export default function ContextMenu({ menu, onClose, onAddRuntime, onAddNode, onDeleteRuntime, onDeleteNode, onHidePort, localRuntimeId }) {
        const supportsNodeCreate = menu.kind === 'runtime' || menu.kind === 'node';
        const firstAvailableGroup = GROUPS.find((key) => Array.isArray(menu[key]) && menu[key].length > 0) || 'sources';

        const groups = useMemo(() => {
                if (!supportsNodeCreate) {
                        return [];
                }
                return GROUPS.map((groupId) => ({
                        id: groupId,
                        label: groupId,
                        items: (Array.isArray(menu[groupId]) ? menu[groupId] : []).map((type) => ({
                                id: type,
                                label: type,
                                onSelect: () => onAddNode(menu.runtimeId, type)
                        }))
                }));
        }, [menu, onAddNode, supportsNodeCreate]);

        const items = useMemo(() => {
                if (supportsNodeCreate) {
                        return [];
                }
                return [{ id: 'new-runtime', label: 'New Runtime', onSelect: () => onAddRuntime() }];
        }, [onAddRuntime, supportsNodeCreate]);

        const actions = useMemo(() => {
                const nextActions = [];
                if (menu.kind === 'runtime' && menu.runtimeId !== localRuntimeId) {
                        nextActions.push({ id: 'delete-runtime', label: 'Delete Runtime', danger: true, onSelect: () => onDeleteRuntime(menu.runtimeId) });
                }
                if (menu.kind === 'node') {
                        if (menu.portName) {
                                nextActions.push({
                                        id: 'hide-port',
                                        label: `hide ${menu.portDirection} ${menu.portName}`,
                                        onSelect: () => onHidePort(menu.nodeId, menu.portDirection, menu.portName)
                                });
                        }
                        nextActions.push({ id: 'delete-node', label: 'delete', danger: true, onSelect: () => onDeleteNode(menu.nodeId) });
                }
                return nextActions;
        }, [localRuntimeId, menu.kind, menu.nodeId, menu.portDirection, menu.portName, menu.runtimeId, onDeleteNode, onDeleteRuntime, onHidePort]);

        return (
                <StandardContextMenu
                        open={menu.open}
                        x={menu.x}
                        y={menu.y}
                        onClose={onClose}
                        groups={groups}
                        items={items}
                        actions={actions}
                        initialGroupId={firstAvailableGroup}
                        groupAriaLabel="node groups"
                        emptyLabel="empty"
                />
        );
}
