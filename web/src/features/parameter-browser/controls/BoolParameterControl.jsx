import React from 'react';

function normalizeBoolValue(value) {
        if (typeof value === 'boolean') {
                return value;
        }
        if (typeof value === 'number') {
                return value !== 0;
        }
        const lowered = String(value ?? '').trim().toLowerCase();
        return lowered === 'true' || lowered === '1' || lowered === 'yes' || lowered === 'on';
}

export default function BoolParameterControl({ item, canEdit, onChange, parameterMeta }) {
        return (
                <input
                        type="checkbox"
                        checked={normalizeBoolValue(item.value)}
                        disabled={!canEdit}
                        onChange={(event) => onChange(item.name, event.target.checked, { ...parameterMeta, interaction: 'immediate' })}
                />
        );
}
