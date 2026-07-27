import React from 'react';

export default function ParameterNameBlock({
        item,
        tooltipText,
        showVisibilityCheckbox,
        parameterVisible,
        onVisibilityChange
}) {
        return (
                <div className="param-name">
                        {showVisibilityCheckbox ? (
                                <input
                                        className="param-visibility-toggle"
                                        type="checkbox"
                                        checked={parameterVisible}
                                        onChange={(event) => {
                                                if (onVisibilityChange) {
                                                        onVisibilityChange(item.name, event.target.checked);
                                                }
                                        }}
                                />
                        ) : null}
                        <label title={tooltipText}>{item.name}</label>
                        {item.origin ? <span className="param-badge">{item.origin}</span> : null}
                </div>
        );
}
