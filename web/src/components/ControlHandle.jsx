import React from 'react';

export default function ControlHandle({ className = '' }) {
        const handleClassName = `ui-control-handle${className ? ` ${className}` : ''}`;

        return <span className={handleClassName} aria-hidden="true" />;
}