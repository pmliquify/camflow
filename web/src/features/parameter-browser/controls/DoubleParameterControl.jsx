import React from 'react';
import Input from '../../../components/Input.jsx';
import Slider from '../../../components/Slider.jsx';

export default function DoubleParameterControl({ item, canEdit, onChange, parameterMeta }) {
        const minimum = Number(item.min ?? item.value ?? 0);
        const maximum = Number(item.max ?? item.value ?? 100);

        return (
                <div className="numeric-control">
                        <Slider
                                min={minimum}
                                max={maximum}
                                step="0.01"
                                value={item.value ?? 0}
                                disabled={!canEdit}
                                onChange={(event) => onChange(item.name, event.target.value, { ...parameterMeta, interaction: 'immediate' })}
                                onFocus={(event) => event.currentTarget.select()}
                                onKeyDown={(event) => {
                                        if (event.key === 'Enter') {
                                                event.preventDefault();
                                        }
                                }}
                                onKeyUp={(event) => {
                                        if (event.key === 'Enter') {
                                                event.currentTarget.blur();
                                                event.currentTarget.focus({ preventScroll: true });
                                        }
                                }}
                        />
                        <Input
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
