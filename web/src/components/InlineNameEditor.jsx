import React, { useEffect, useRef, useState } from 'react';
import Input from './Input.jsx';

export default function InlineNameEditor({ value, onCommit, className = '', inputClassName = '', ariaLabel = 'name' }) {
        const [editing, setEditing] = useState(false);
        const [draft, setDraft] = useState(String(value || ''));
        const finishingRef = useRef(false);

        useEffect(() => {
                if (!editing) {
                        setDraft(String(value || ''));
                }
        }, [editing, value]);

        const finish = (commit) => {
                if (finishingRef.current) {
                        return;
                }
                finishingRef.current = true;
                const nextValue = draft.trim();
                setEditing(false);
                if (commit && nextValue && nextValue !== value) {
                        onCommit(nextValue);
                }
                window.setTimeout(() => {
                        finishingRef.current = false;
                }, 0);
        };

        if (editing) {
                return (
                        <Input
                                className={inputClassName}
                                type="text"
                                value={draft}
                                aria-label={ariaLabel}
                                autoFocus
                                onFocus={(event) => event.currentTarget.select()}
                                onMouseDown={(event) => event.stopPropagation()}
                                onClick={(event) => event.stopPropagation()}
                                onChange={(event) => setDraft(event.target.value)}
                                onBlur={() => finish(true)}
                                onKeyDown={(event) => {
                                        if (event.key === 'Enter') {
                                                event.preventDefault();
                                                finish(true);
                                        } else if (event.key === 'Escape') {
                                                event.preventDefault();
                                                finish(false);
                                        }
                                }}
                        />
                );
        }

        return (
                <span
                        className={className}
                        title={String(value || '')}
                        onDoubleClick={(event) => {
                                event.stopPropagation();
                                setDraft(String(value || ''));
                                setEditing(true);
                        }}
                >
                        {value}
                </span>
        );
}