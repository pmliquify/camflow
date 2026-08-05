import React from 'react';
import Button from '../../../components/Button.jsx';

export default function ButtonParameterControl({ item, canEdit, onChange, parameterMeta }) {
        return (
                <Button
                        type="button"
                        compact={true}
                        disabled={!canEdit}
                        onClick={() => onChange(item.name, '1', { ...parameterMeta, interaction: 'immediate' })}
                >
                        Trigger
                </Button>
        );
}
