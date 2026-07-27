import React, { useEffect, useState } from 'react';

export default function IntParameterControl({ item, canEdit, onChange, parameterMeta }) {
        const [intDraftValue, setIntDraftValue] = useState(String(item.value ?? ''));

        useEffect(() => {
                setIntDraftValue(String(item.value ?? ''));
        }, [item.value, item.name]);

        function commitIntDraftValue() {
                onChange(item.name, intDraftValue, { ...parameterMeta, interaction: 'number-commit' });
        }

        return (
                <div className="numeric-control">
                        <input
                                type="range"
                                min={item.min ?? item.value ?? 0}
                                max={item.max ?? item.value ?? 100}
                                step="1"
                                value={item.value ?? 0}
                                disabled={!canEdit}
                                onChange={(event) => {
                                        const nextValue = event.target.value;
                                        setIntDraftValue(nextValue);
                                        onChange(item.name, nextValue, { ...parameterMeta, interaction: 'slider' });
                                }}
                        />
                        <input
                                type="number"
                                min={item.min ?? item.value ?? 0}
                                max={item.max ?? item.value ?? 100}
                                step="1"
                                value={intDraftValue}
                                disabled={!canEdit}
                                onChange={(event) => setIntDraftValue(event.target.value)}
                                onBlur={commitIntDraftValue}
                                onKeyDown={(event) => {
                                        if (event.key === 'Enter') {
                                                event.preventDefault();
                                                commitIntDraftValue();
                                                event.currentTarget.blur();
                                        }
                                }}
                        />
                </div>
        );
}
