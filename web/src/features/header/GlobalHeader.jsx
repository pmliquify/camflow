import React from 'react';
import logoSelected from '../../assets/logos/camflow_icon.svg';
import Button from '../../components/Button.jsx';
import Label from '../../components/Label.jsx';

export default function GlobalHeader({
        runtimeStatusText,
        versionParts,
        graphStatusText,
        runtimeRunning,
        onToggleRuntime,
        viewMode,
        onSetViewMode
}) {
        const normalizedSecondary = versionParts.secondary
                ? (versionParts.secondary.startsWith('|')
                        ? ` | ${versionParts.secondary.slice(1).trimStart()}`
                        : `  ${versionParts.secondary}`)
                : '';

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
                                        <span className="header-status-text" title={graphStatusText || ''}>{graphStatusText}</span>
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
                        </div>
                </header>
        );
}
