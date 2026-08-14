import React, { useEffect, useRef, useState } from 'react';
import Input from '../../../components/Input.jsx';

export default function StringParameterControl({ item, canEdit, onChange, parameterMeta }) {
        const [stringDraftValue, setStringDraftValue] = useState(String(item.value ?? ''));
        const suppressNextBlurRef = useRef(false);

        useEffect(() => {
                setStringDraftValue(String(item.value ?? ''));
        }, [item.name, item.value]);

        function commitStringDraftValue() {
                return onChange(item.name, stringDraftValue, { ...parameterMeta, interaction: 'string-commit' });
        }

        return (
                <Input
                        type="text"
                        value={stringDraftValue}
                        disabled={!canEdit}
                        onChange={(event) => setStringDraftValue(event.target.value)}
                        onFocus={(event) => event.currentTarget.select()}
                        onBlur={() => {
                                if (suppressNextBlurRef.current) {
                                        suppressNextBlurRef.current = false;
                                        return;
                                }
                                void commitStringDraftValue();
                        }}
                        onKeyDown={(event) => {
                                if (event.key === 'Enter') {
                                        event.preventDefault();
                                        void commitStringDraftValue();
                                }
                        }}
                        onKeyUp={(event) => {
                                if (event.key === 'Enter') {
                                        suppressNextBlurRef.current = true;
                                        event.currentTarget.blur();
                                        event.currentTarget.focus({ preventScroll: true });
                                }
                        }}
                />
        );
}
