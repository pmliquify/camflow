import React from 'react';
import ControlHandle from './ControlHandle.jsx';

export default function Checkbox({ className = '', children, ...props }) {
        const checkboxClassName = `ui-checkbox${className ? ` ${className}` : ''}`;

        return (
                <label className={checkboxClassName}>
                        <input {...props} className="ui-checkbox-input" type="checkbox" />
                        <span className="ui-checkbox-track" aria-hidden="true">
                                <ControlHandle className="ui-checkbox-handle" />
                        </span>
                        {children}
                </label>
        );
}
