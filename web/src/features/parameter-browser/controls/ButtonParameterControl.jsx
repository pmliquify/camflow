import React from 'react';
import UiButton from '../../../components/UiButton.jsx';

export default function ButtonParameterControl({ item, canEdit, onChange, parameterMeta }) {
        return (
                <UiButton
                        type="button"
                        compact={true}
                        disabled={!canEdit}
                        onClick={() => onChange(item.name, '1', { ...parameterMeta, interaction: 'immediate' })}
                >
                        Trigger
                </UiButton>
        );
}
