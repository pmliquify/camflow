import React from 'react';

export default function UiButton({
        variant = 'primary',
        icon = null,
        compact = false,
        iconOnly = false,
        className = '',
        children,
        ...buttonProps
}) {
        const resolvedClassName = [
                'ui-button',
                variant === 'secondary' ? 'secondary' : '',
                compact ? 'ui-button-compact' : '',
                iconOnly ? 'ui-button-icon-only' : '',
                className
        ].filter(Boolean).join(' ');

        const iconElement = typeof icon === 'string'
                ? <img src={icon} alt="" aria-hidden="true" className="button-icon" />
                : icon;

        return (
                <button {...buttonProps} className={resolvedClassName}>
                        {iconElement}
                        {children ? <span className="ui-button-label">{children}</span> : null}
                </button>
        );
}
