import React from 'react';
import UiButton from '../../components/UiButton.jsx';

export default function NodeActions({ beginEdgeConnection, selectedNodeId, deleteNode, pendingEdgeSourceId }) {
        return (
                <div className="selection-actions">
                        <UiButton className="secondary" variant="secondary" type="button" onClick={() => beginEdgeConnection(selectedNodeId)}>connect</UiButton>
                        <UiButton className="secondary" variant="secondary" type="button" onClick={() => void deleteNode(selectedNodeId)}>delete node</UiButton>
                        {pendingEdgeSourceId ? <span className="edge-pending">from {pendingEdgeSourceId}</span> : null}
                </div>
        );
}
