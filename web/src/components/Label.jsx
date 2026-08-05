import React from 'react';

export default function Label({
        tone = 'neutral',
        size = 'small',
        uppercase = false,
        iconOnly = false,
        className = '',
        children,
        ...labelProps
}) {
        const resolvedClassName = [
                'ui-label',
                `ui-label-${tone}`,
                `ui-label-${size}`,
                uppercase ? 'ui-label-uppercase' : '',
                iconOnly ? 'ui-label-icon-only' : '',
                className
        ].filter(Boolean).join(' ');

        return <span {...labelProps} className={resolvedClassName}>{children}</span>;
}