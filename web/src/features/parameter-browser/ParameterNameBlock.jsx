import React from 'react';
import reloadIcon from '../../assets/images/icon-reload.svg';
import Checkbox from '../../components/Checkbox.jsx';

export default function ParameterNameBlock({
        item,
        displayName,
        tooltipText,
        showVisibilityCheckbox,
        parameterVisible,
        onVisibilityChange
}) {
        return (
                <div className="param-name">
                        {showVisibilityCheckbox ? (
                                <Checkbox
                                        className="param-visibility-toggle"
                                        checked={parameterVisible}
                                        aria-label={`Show ${displayName || item.name}`}
                                        onChange={(event) => {
                                                if (onVisibilityChange) {
                                                        onVisibilityChange(item.name, event.target.checked);
                                                }
                                        }}
                                />
                        ) : null}
                        <label title={tooltipText}>{displayName || item.name}</label>
                        {item.hasSideEffects ? (
                                <span className="param-side-effect-marker" title="reload required for dependent parameters">
                                        <img src={reloadIcon} alt="reload" />
                                </span>
                        ) : null}
                        {item.origin ? <span className="param-badge">{item.origin}</span> : null}
                </div>
        );
}
