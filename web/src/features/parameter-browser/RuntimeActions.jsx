import React from 'react';
import Button from '../../components/Button.jsx';

export default function RuntimeActions({ selectedRuntimeId, deleteRuntime }) {
        return (
                <div className="selection-actions">
                        <Button className="secondary" variant="secondary" type="button" onClick={() => void deleteRuntime(selectedRuntimeId)}>delete runtime</Button>
                </div>
        );
}
