import React from 'react';

const EXCLUDED_TOP_LEVEL_KEYS = new Set(['nodeId', 'hasImage', 'image', 'images', 'keys']);

function isSimpleValue(value) {
        return value == null || typeof value === 'string' || typeof value === 'number' || typeof value === 'boolean';
}

function toDisplayValue(value) {
        if (value == null) {
                return 'null';
        }
        if (typeof value === 'boolean') {
                return value ? 'true' : 'false';
        }
        return String(value);
}

function pushSimpleEntries(target, source, prefix = '') {
        if (!source || typeof source !== 'object' || Array.isArray(source)) {
                return;
        }
        Object.entries(source).forEach(([key, value]) => {
                if (!isSimpleValue(value)) {
                        return;
                }
                target.push({
                        key: prefix ? `${prefix}.${key}` : key,
                        value: toDisplayValue(value)
                });
        });
}

function extractSimpleEntries(frameContextState) {
        const entries = [];
        const seen = new Set();

        const addEntry = (entry) => {
                const id = `${entry.key}=${entry.value}`;
                if (seen.has(id)) {
                        return;
                }
                seen.add(id);
                entries.push(entry);
        };

        const scopedValues = frameContextState?.values;
        if (scopedValues && typeof scopedValues === 'object' && !Array.isArray(scopedValues)) {
                Object.entries(scopedValues).forEach(([scope, valueMap]) => {
                        const scopedEntries = [];
                        pushSimpleEntries(scopedEntries, valueMap, scope);
                        scopedEntries.forEach(addEntry);
                });
        }

        const scalarValues = frameContextState?.scalars;
        if (scalarValues && typeof scalarValues === 'object' && !Array.isArray(scalarValues)) {
                const scalarEntries = [];
                pushSimpleEntries(scalarEntries, scalarValues);
                scalarEntries.forEach(addEntry);
        }

        if (frameContextState && typeof frameContextState === 'object' && !Array.isArray(frameContextState)) {
                Object.entries(frameContextState).forEach(([key, value]) => {
                        if (EXCLUDED_TOP_LEVEL_KEYS.has(key)) {
                                return;
                        }
                        if (!isSimpleValue(value)) {
                                return;
                        }
                        addEntry({ key, value: toDisplayValue(value) });
                });
        }

        return entries;
}

export default function FrameContextValues({ frameContextState }) {
        const entries = extractSimpleEntries(frameContextState);

        if (entries.length === 0) {
                return null;
        }

        return (
                <div className="value-table compact-values">
                        {entries.map((entry) => (
                                <div key={`${entry.key}:${entry.value}`} className="value-row">
                                        <span title={entry.key}>{entry.key}</span>
                                        <span title={entry.value}>{entry.value}</span>
                                </div>
                        ))}
                </div>
        );
}
