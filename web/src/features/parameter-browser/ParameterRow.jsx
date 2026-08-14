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

        const groupName = String(item.group || '').trim();
        const fullName = String(item.name || '');
        const groupPrefix = groupName ? `${groupName}.` : '';
        const displayName = groupPrefix && fullName.startsWith(groupPrefix) ? fullName.slice(groupPrefix.length) : fullName;


        return (
                <section
                        className={`parameter${item.type === 'media-flags' ? ' media-flags-parameter' : ''}`}
                        title={tooltipText}
                        tabIndex={-1}
                        data-parameter-name={displayName || item.name}
                        data-parameter-full-name={item.name}
                >
                        <div className="parameter-row">
                                <ParameterNameBlock
                                        item={item}
                                        displayName={displayName}
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
