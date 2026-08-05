import React, { useEffect, useState } from 'react';
import Input from '../../../components/Input.jsx';

export default function StringParameterControl({ item, canEdit, onChange, parameterMeta }) {
        const [stringDraftValue, setStringDraftValue] = useState(String(item.value ?? ''));

        useEffect(() => {
                setStringDraftValue(String(item.value ?? ''));
        }, [item.name, item.value]);

        function commitStringDraftValue() {
                onChange(item.name, stringDraftValue, { ...parameterMeta, interaction: 'string-commit' });
        }

        return (
                <Input
                        type="text"
                        value={stringDraftValue}
                        disabled={!canEdit}
                        onChange={(event) => setStringDraftValue(event.target.value)}
                        onBlur={commitStringDraftValue}
                        onKeyDown={(event) => {
                                if (event.key === 'Enter') {
                                        event.preventDefault();
                                        commitStringDraftValue();
                                        event.currentTarget.blur();
                                }
                        }}
                />
        );
}
