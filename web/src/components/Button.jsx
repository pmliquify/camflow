import React, { forwardRef } from 'react';

const Button = forwardRef(function Button({
        variant = 'primary',
        icon = null,
        compact = false,
        iconOnly = false,
        className = '',
        children,
        ...buttonProps
}, ref) {
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
                <button {...buttonProps} ref={ref} className={resolvedClassName}>
                        {iconElement}
                        {children ? <span className="ui-button-label">{children}</span> : null}
                </button>
        );
});

export default Button;