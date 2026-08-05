import React from 'react';
import reloadIcon from '../../assets/images/icon-reload.svg';
import Checkbox from '../../components/Checkbox.jsx';
import Label from '../../components/Label.jsx';
import { isLogarithmicIntParameter } from './controls/intParameterScale.js';

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
                        {item.origin ? <Label uppercase={true}>{item.origin}</Label> : null}
                        {isLogarithmicIntParameter(item) ? <Label title="logarithmic scale">log</Label> : null}
                        {item.hasSideEffects ? (
                                <Label iconOnly={true} title="reload required for dependent parameters" aria-label="reload required for dependent parameters">
                                        <img src={reloadIcon} alt="" aria-hidden="true" />
                                </Label>
                        ) : null}
                </div>
        );
}
