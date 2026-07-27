import React from 'react';
import UiButton from '../../components/UiButton.jsx';

export default function RuntimeActions({ selectedRuntimeId, deleteRuntime }) {
        return (
                <div className="selection-actions">
                        <UiButton className="secondary" variant="secondary" type="button" onClick={() => void deleteRuntime(selectedRuntimeId)}>delete runtime</UiButton>
                </div>
        );
}
