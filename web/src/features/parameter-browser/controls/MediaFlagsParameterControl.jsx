import React from 'react';
import { mediaFlagEntries, mediaFlagsHex } from '../../../utils/mediaFlags.js';

export default function MediaFlagsParameterControl({ item }) {
        const hexValue = mediaFlagsHex(item.value);
        const entries = mediaFlagEntries(item.flagKind, item.value);
        const accessibleValue = entries.map((entry) => `${entry.label}: ${entry.set ? 'set' : 'not set'}`).join(', ');

        return (
                <div className="media-flags-value" aria-label={`${hexValue}; ${accessibleValue}`}>
                        <span className="media-flags-hex">{hexValue}</span>
                        {' ('}
                        {entries.map((entry, index) => (
                                <React.Fragment key={entry.label}>
                                        {index > 0 ? ' | ' : null}
                                        <span className={entry.set ? 'media-flag-set' : 'media-flag-unset'} aria-label={`${entry.label}: ${entry.set ? 'set' : 'not set'}`}>
                                                {entry.set ? entry.label : <s>{entry.label}</s>}
                                        </span>
                                </React.Fragment>
                        ))}
                        {')'}
                </div>
        );
}
