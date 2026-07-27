import React from 'react';

export default function DoubleParameterControl({ item, canEdit, onChange, parameterMeta }) {
        return (
                <div className="numeric-control">
                        <input
                                type="range"
                                min={item.min ?? item.value ?? 0}
                                max={item.max ?? item.value ?? 100}
                                step="0.01"
                                value={item.value ?? 0}
                                disabled={!canEdit}
                                onChange={(event) => onChange(item.name, event.target.value, { ...parameterMeta, interaction: 'immediate' })}
                        />
                        <input
                                type="number"
                                min={item.min ?? item.value ?? 0}
                                max={item.max ?? item.value ?? 100}
                                step="0.01"
                                value={item.value ?? 0}
                                disabled={!canEdit}
                                onChange={(event) => onChange(item.name, event.target.value, { ...parameterMeta, interaction: 'immediate' })}
                        />
                </div>
        );
}
