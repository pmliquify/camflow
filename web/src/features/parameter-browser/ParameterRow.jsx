import React from 'react';
import ParameterNameBlock from './ParameterNameBlock.jsx';
import ParameterValueControl from './ParameterValueControl.jsx';

export default function ParameterRow({ item, canEdit, onChange, showVisibilityCheckbox = false, parameterVisible = true, onVisibilityChange }) {
        const parameterMeta = {
                parameterType: item.type,
                hasSideEffects: Boolean(item.hasSideEffects)
        };

        const tooltipParts = [];
        const descriptionText = String(item.description || '').trim();
        if (descriptionText) {
                tooltipParts.push(descriptionText);
        } else {
                tooltipParts.push('no description');
        }
        if (item.type) {
                tooltipParts[0] = `${tooltipParts[0]} (${item.type})`;
        }
        if (item.source) {
                tooltipParts.push(`source: ${item.source}`);
        }
        if (item.origin) {
                tooltipParts.push(`origin: ${item.origin}`);
        }
        const tooltipText = tooltipParts.join(' | ');


        return (
                <section className="parameter" title={tooltipText}>
                        <div className="parameter-row">
                                <ParameterNameBlock
                                        item={item}
                                        tooltipText={tooltipText}
                                        showVisibilityCheckbox={showVisibilityCheckbox}
                                        parameterVisible={parameterVisible}
                                        onVisibilityChange={onVisibilityChange}
                                />
                                <ParameterValueControl item={item} canEdit={canEdit} onChange={onChange} parameterMeta={parameterMeta} />
                        </div>
                </section>
        );
}
