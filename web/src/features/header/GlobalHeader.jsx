import React, { useEffect, useRef } from 'react';
import logoSelected from '../../assets/logos/camflow_icon.svg';
import keyboardIcon from '../../assets/images/icon-keyboard.svg';
import Button from '../../components/Button.jsx';
import Label from '../../components/Label.jsx';

export default function GlobalHeader({
        runtimeStatusText,
        versionParts,
        graphStatusText,
        statusError = false,
        runtimeRunning,
        onToggleRuntime,
        viewMode,
        onSetViewMode,
        shortcutPanelOpen,
        onSetShortcutPanelOpen
}) {
        const shortcutPanelRef = useRef(null);
        const shortcutButtonRef = useRef(null);
        const shortcutModifier = /Mac|iPhone|iPad|iPod/.test(navigator.platform) ? '⌥' : 'Alt';
        const normalizedSecondary = versionParts.secondary
                ? (versionParts.secondary.startsWith('|')
                        ? ` | ${versionParts.secondary.slice(1).trimStart()}`
                        : `  ${versionParts.secondary}`)
                : '';

        useEffect(() => {
                if (!shortcutPanelOpen) {
                        return undefined;
                }

                const pointerDownHandler = (event) => {
                        if (!shortcutPanelRef.current?.contains(event.target) && !shortcutButtonRef.current?.contains(event.target)) {
                                onSetShortcutPanelOpen(false);
                        }
                };
                const keydownHandler = (event) => {
                        if (event.key === 'Escape') {
                                onSetShortcutPanelOpen(false);
                                shortcutButtonRef.current?.focus();
                        }
                };
                document.addEventListener('pointerdown', pointerDownHandler);
                document.addEventListener('keydown', keydownHandler);
                return () => {
                        document.removeEventListener('pointerdown', pointerDownHandler);
                        document.removeEventListener('keydown', keydownHandler);
                };
        }, [onSetShortcutPanelOpen, shortcutPanelOpen]);

        return (
                <header className="topbar" onClick={(event) => event.stopPropagation()}>
                        <div className="brand">
                                <img className="logo-mark" src={logoSelected} alt="camflow logo" />
                                <div className="brand-line">
                                        <strong>
                                                camflow
                                                <span className="runtime-version runtime-version-compact" tabIndex={0}>
                                                        <span className="runtime-version-primary"> {versionParts.primary}</span>
                                                        <span className="runtime-version-tooltip" role="tooltip" aria-label="full runtime version">
                                                                <span className="runtime-version-primary">{versionParts.primary}</span>
                                                                {normalizedSecondary ? <span className="runtime-version-secondary">{normalizedSecondary}</span> : null}
                                                        </span>
                                                </span>
                                        </strong>
                                        <Label
                                                size="medium"
                                                tone={runtimeStatusText === 'running' ? 'success' : runtimeStatusText === 'down' ? 'danger' : 'info'}
                                        >
                                                {runtimeStatusText}
                                        </Label>
                                        <span className={`header-status-text${statusError ? ' is-error' : ''}`} title={graphStatusText || ''}>{graphStatusText}</span>
                                </div>
                        </div>
                        <div className="topbar-meta">
                                <Button className="topbar-runtime-toggle" type="button" onClick={onToggleRuntime}>
                                        {runtimeRunning ? 'Stop' : 'Start'}
                                </Button>
                                <div className="mode-switch">
                                        <Button className={viewMode === 'viewer' ? 'active' : ''} type="button" onClick={() => onSetViewMode('viewer')}>viewer</Button>
                                        <Button className={viewMode === 'editor' ? 'active' : ''} type="button" onClick={() => onSetViewMode('editor')}>editor</Button>
                                </div>
                                <div className="shortcut-help">
                                        <Button
                                                ref={shortcutButtonRef}
                                                className={`shortcut-help-button${shortcutPanelOpen ? ' active' : ''}`}
                                                type="button"
                                                icon={keyboardIcon}
                                                iconOnly={true}
                                                aria-label="Show keyboard shortcuts"
                                                aria-expanded={shortcutPanelOpen}
                                                aria-controls="keyboard-shortcuts-panel"
                                                title="Keyboard shortcuts"
                                                onClick={() => onSetShortcutPanelOpen((open) => !open)}
                                        />
                                        {shortcutPanelOpen ? (
                                                <div ref={shortcutPanelRef} id="keyboard-shortcuts-panel" className="shortcut-popover" role="dialog" aria-label="Keyboard shortcuts">
                                                        <div className="shortcut-popover-title">Keyboard shortcuts</div>
                                                        <div className="shortcut-row">
                                                                <span>Switch viewer / editor</span>
                                                                <span className="shortcut-keys"><kbd>{shortcutModifier}</kbd><span>+</span><kbd>V</kbd></span>
                                                        </div>
                                                        <div className="shortcut-row">
                                                                <span>Start / stop runtime</span>
                                                                <span className="shortcut-keys"><kbd>{shortcutModifier}</kbd><span>+</span><kbd>R</kbd></span>
                                                        </div>
                                                        <div className="shortcut-row">
                                                                <span>Open shortcut help</span>
                                                                <span className="shortcut-keys"><kbd>{shortcutModifier}</kbd><span>+</span><kbd>H</kbd></span>
                                                        </div>
                                                        <div className="shortcut-row">
                                                                <span>Open parameter filter</span>
                                                                <span className="shortcut-keys"><kbd>{shortcutModifier}</kbd><span>+</span><kbd>F</kbd></span>
                                                        </div>
                                                        <div className="shortcut-row">
                                                                <span>Clear selected runtime log</span>
                                                                <span className="shortcut-keys"><kbd>{shortcutModifier}</kbd><span>+</span><kbd>K</kbd></span>
                                                        </div>
                                                        <div className="shortcut-row">
                                                                <span>Open selected runtime log filter</span>
                                                                <span className="shortcut-keys"><kbd>{shortcutModifier}</kbd><span>+</span><kbd>G</kbd></span>
                                                        </div>
                                                </div>
                                        ) : null}
                                </div>
                        </div>
                </header>
        );
}
