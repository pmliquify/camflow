import React from 'react';
import logoSelected from '../../assets/logos/camflow_icon.svg';
import UiButton from '../../components/UiButton.jsx';

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
                                        <span className={`runtime-pill ${runtimeStatusText}`}>{runtimeStatusText}</span>
                                        <span className="header-status-text" title={graphStatusText || ''}>{graphStatusText}</span>
                                </div>
                        </div>
                        <div className="topbar-meta">
                                <UiButton className="topbar-runtime-toggle" type="button" onClick={onToggleRuntime}>
                                        {runtimeRunning ? 'Stop' : 'Start'}
                                </UiButton>
                                <div className="mode-switch">
                                        <UiButton className={viewMode === 'viewer' ? 'active' : ''} type="button" onClick={() => onSetViewMode('viewer')}>viewer</UiButton>
                                        <UiButton className={viewMode === 'editor' ? 'active' : ''} type="button" onClick={() => onSetViewMode('editor')}>editor</UiButton>
                                </div>
                        </div>
                </header>
        );
}
