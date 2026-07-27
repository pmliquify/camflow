import React from 'react';

export default function OptionParameterControl({ item, canEdit, onChange, parameterMeta }) {
        return (
                <select disabled={!canEdit} value={item.value ?? ''} onChange={(event) => onChange(item.name, event.target.value, { ...parameterMeta, interaction: 'immediate' })}>
                        {(item.options || []).map((option, index) => (
                                <option key={option} value={option}>{item.optionLabels?.[index] || option}</option>
                        ))}
                </select>
        );
}
