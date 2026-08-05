import React, { forwardRef } from 'react';

const Input = forwardRef(function Input({ onDoubleClick, ...inputProps }, ref) {
        return (
                <input
                        {...inputProps}
                        ref={ref}
                        onDoubleClick={(event) => {
                                event.currentTarget.select();
                                if (onDoubleClick) {
                                        onDoubleClick(event);
                                }
                        }}
                />
        );
});

export default Input;