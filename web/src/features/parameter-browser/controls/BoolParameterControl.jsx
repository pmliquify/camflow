import React from 'react';
import Checkbox from '../../../components/Checkbox.jsx';

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
                <Checkbox
                        className="bool-parameter-control"
                        checked={normalizeBoolValue(item.value)}
                        disabled={!canEdit}
                        aria-label={item.name}
                        onChange={(event) => onChange(item.name, event.target.checked, { ...parameterMeta, interaction: 'immediate' })}
                />
        );
}
