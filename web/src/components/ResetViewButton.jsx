import React from 'react';
import resetViewIcon from '../assets/images/icon-reset-view.svg';
import Button from './Button.jsx';

export default function ResetViewButton({ onClick, className = '', ariaLabel = 'reset view', title = 'reset view' }) {
        return (
                <Button
                        variant="secondary"
                        type="button"
                        className={`tool-chip tool-chip-round tool-chip-reset ${className}`.trim()}
                        icon={resetViewIcon}
                        iconOnly={true}
                        onClick={onClick}
                        aria-label={ariaLabel}
                        title={title}
                />
        );
}
