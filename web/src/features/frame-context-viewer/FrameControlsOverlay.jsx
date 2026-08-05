import React from 'react';
import { isColorRawFormat } from '../../services/frameRendering.js';
import bayerIcon from '../../assets/images/icon-bayer.svg';
import ResetViewButton from '../../components/ResetViewButton.jsx';
import Button from '../../components/Button.jsx';

export default function FrameControlsOverlay({
        hasFrame,
        frameMeta,
        debayerEnabled,
        setDebayerEnabled,
        onResetView
}) {
        const formatId = Number(frameMeta?.formatId);
        const showDebayerControl = hasFrame && isColorRawFormat(formatId);

        return (
                <div
                        className="frame-overlay-controls"
                >
                        {showDebayerControl ? (
                                <Button className={`tool-chip tool-chip-round ${debayerEnabled ? 'active' : ''}`} type="button" aria-pressed={debayerEnabled} icon={bayerIcon} iconOnly={true} onClick={() => setDebayerEnabled((value) => !value)} />
                        ) : null}
                        <ResetViewButton onClick={onResetView} />
                </div>
        );
}
