import React, { useEffect, useMemo, useRef, useState } from 'react';
import Checkbox from '../../../components/Checkbox.jsx';

function parseMultiSelectValue(value) {
        if (Array.isArray(value)) {
                return value.map((entry) => String(entry)).filter((entry) => entry.length > 0);
        }

        const raw = String(value ?? '');
        if (!raw) {
                return [];
        }

        return raw
                .split(',')
                .map((entry) => entry.trim())
                .filter((entry) => entry.length > 0);
}

export default function OptionParameterControl({ item, canEdit, onChange, parameterMeta }) {
        const multiSelect = Boolean(item.multiSelect);
        const selectedValues = multiSelect ? parseMultiSelectValue(item.value) : [String(item.value ?? '')];
        const [isOpen, setIsOpen] = useState(false);
        const rootRef = useRef(null);

        useEffect(() => {
                if (!canEdit) {
                        setIsOpen(false);
                }
        }, [canEdit]);

        useEffect(() => {
                if (!multiSelect || !isOpen) {
                        return;
                }

                const handlePointerDown = (event) => {
                        if (!rootRef.current || rootRef.current.contains(event.target)) {
                                return;
                        }
                        setIsOpen(false);
                };

                document.addEventListener('mousedown', handlePointerDown);
                return () => {
                        document.removeEventListener('mousedown', handlePointerDown);
                };
        }, [multiSelect, isOpen]);

        const optionEntries = useMemo(
                () => (item.options || []).map((option, index) => ({ value: option, label: item.optionLabels?.[index] || option })),
                [item.options, item.optionLabels]
        );

        const selectedSet = useMemo(() => new Set(selectedValues), [selectedValues]);
        const firstSelectedLabel = useMemo(() => {
                for (const selectedValue of selectedValues) {
                        const match = optionEntries.find((entry) => entry.value === selectedValue);
                        if (match) {
                                return match.label;
                        }
                }
                return '';
        }, [selectedValues, optionEntries]);

        const handleChange = (event) => {
                if (!multiSelect) {
                        onChange(item.name, event.target.value, { ...parameterMeta, interaction: 'immediate' });
                        return;
                }

                const selected = Array.from(event.target.selectedOptions || []).map((option) => option.value);
                onChange(item.name, selected.join(','), { ...parameterMeta, interaction: 'immediate' });
        };

        if (!multiSelect) {
                return (
                        <select
                                disabled={!canEdit}
                                value={selectedValues[0]}
                                onChange={handleChange}
                        >
                                {(item.options || []).map((option, index) => (
                                        <option key={option} value={option}>{item.optionLabels?.[index] || option}</option>
                                ))}
                        </select>
                );
        }

        const toggleOption = (optionValue) => {
                const nextSelected = optionEntries
                        .map((entry) => entry.value)
                        .filter((value) => (value === optionValue ? !selectedSet.has(value) : selectedSet.has(value)));
                onChange(item.name, nextSelected.join(','), { ...parameterMeta, interaction: 'immediate' });
        };

        return (
                <div ref={rootRef} className={`multi-option-dropdown${isOpen ? ' is-open' : ''}`}>
                        <button
                                className="multi-option-trigger"
                                type="button"
                                disabled={!canEdit}
                                onClick={() => setIsOpen((open) => !open)}
                                title={firstSelectedLabel || 'No subdevices selected'}
                        >
                                <span className="multi-option-trigger-text">{firstSelectedLabel || 'select subdevices'}</span>
                                <span className="multi-option-trigger-caret" aria-hidden="true">▾</span>
                        </button>
                        {isOpen ? (
                                <div className="multi-option-menu">
                                        {optionEntries.map((entry) => (
                                                <Checkbox
                                                        key={entry.value}
                                                        className="multi-option-entry"
                                                        title={entry.label}
                                                        aria-label={entry.label}
                                                        checked={selectedSet.has(entry.value)}
                                                        disabled={!canEdit}
                                                        onChange={() => toggleOption(entry.value)}
                                                >
                                                        <span>{entry.label}</span>
                                                </Checkbox>
                                        ))}
                                </div>
                        ) : null}
                </div>
        );
}
