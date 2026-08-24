import React, { forwardRef } from 'react';

// Some keyboard layouts still commit a character for Alt shortcuts even when the keydown default is
// suppressed, so value changes are ignored for a short moment after an Alt shortcut was pressed.
const ALT_SHORTCUT_GUARD_MS = 300;
let altShortcutGuardUntil = 0;
let guardedSelection = null;

function readSelection(element) {
        try {
                // Number inputs report null and reject setSelectionRange, so the range is restored with select().
                return { start: element.selectionStart, end: element.selectionEnd, direction: element.selectionDirection };
        } catch (_) {
                return { start: null, end: null, direction: null };
        }
}

function restoreGuardedSelection(element) {
        if (!guardedSelection || guardedSelection.element !== element || document.activeElement !== element) {
                return;
        }
        const { start, end, direction } = guardedSelection;
        if (start === null || end === null) {
                element.select();
                return;
        }
        try {
                element.setSelectionRange(start, end, direction || undefined);
        } catch (_) {
                element.select();
        }
}

if (typeof window !== 'undefined') {
        window.addEventListener('keydown', (event) => {
                if (event.altKey && !event.ctrlKey && !event.metaKey && String(event.code || '').startsWith('Key')) {
                        altShortcutGuardUntil = Date.now() + ALT_SHORTCUT_GUARD_MS;
                        const active = document.activeElement;
                        guardedSelection = active instanceof HTMLInputElement
                                ? { element: active, ...readSelection(active) }
                                : null;
                }
        }, true);
        window.addEventListener('beforeinput', (event) => {
                if (Date.now() < altShortcutGuardUntil && String(event.inputType || '').startsWith('insert')) {
                        event.preventDefault();
                }
        }, true);
}

const Input = forwardRef(function Input({ onChange, onDoubleClick, ...inputProps }, ref) {
        return (
                <input
                        {...inputProps}
                        ref={ref}
                        onChange={(event) => {
                                if (Date.now() < altShortcutGuardUntil) {
                                        const element = event.currentTarget;
                                        // React rewrites the controlled value after this handler, which drops the selection.
                                        window.setTimeout(() => restoreGuardedSelection(element), 0);
                                        return;
                                }
                                onChange?.(event);
                        }}
                        onDoubleClick={(event) => {
                                event.currentTarget.select();
                                if (onDoubleClick) {
                                        onDoubleClick(event);
                                }
                        }}
                />
        );
});

export default Input;