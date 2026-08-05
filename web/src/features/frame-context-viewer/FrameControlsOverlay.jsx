import React from 'react';
import { isColorRawFormat, isRawFormat } from '../../services/frameRendering.js';
import bayerIcon from '../../assets/images/icon-bayer.svg';
import ResetViewButton from '../../components/ResetViewButton.jsx';
import Slider from '../../components/Slider.jsx';
import Button from '../../components/Button.jsx';

function bitshiftLabel(value) {
        if (value >= 8) {
                return 'MSB';
        }
        if (value <= 0) {
                return 'LSB';
        }
        return `shift: ${value}`;
}

export default function FrameControlsOverlay({
        viewerControlsOpen,
        setViewerControlsOpen,
        hasFrame,
        frameMeta,
        currentShiftValue,
        setCurrentShiftValue,
        debayerEnabled,
        setDebayerEnabled,
        onResetView
}) {
        const hideShiftPanelTimerRef = React.useRef(null);

        const openShiftPanel = () => {
                if (hideShiftPanelTimerRef.current) {
                        window.clearTimeout(hideShiftPanelTimerRef.current);
                        hideShiftPanelTimerRef.current = null;
                }
                setViewerControlsOpen(true);
        };

        const closeShiftPanel = () => {
                if (hideShiftPanelTimerRef.current) {
                        window.clearTimeout(hideShiftPanelTimerRef.current);
                }
                hideShiftPanelTimerRef.current = window.setTimeout(() => {
                        setViewerControlsOpen(false);
                        hideShiftPanelTimerRef.current = null;
                }, 140);
        };

        React.useEffect(() => {
                return () => {
                        if (hideShiftPanelTimerRef.current) {
                                window.clearTimeout(hideShiftPanelTimerRef.current);
                                hideShiftPanelTimerRef.current = null;
                        }
                };
        }, []);

        const currentBitshiftLabel = bitshiftLabel(currentShiftValue);
        const formatId = Number(frameMeta?.formatId);
        const bitsPerPixel = Number(frameMeta?.bitsPerPixel) || 0;
        const showDebayerControl = hasFrame && isColorRawFormat(formatId);
        const showBitshiftControl = hasFrame && isRawFormat(formatId) && bitsPerPixel > 8;

        return (
                <div
                        className="frame-overlay-controls"
                >
                        {showBitshiftControl ? (
                                <Button
                                        className={`tool-chip tool-chip-shift ${currentShiftValue >= 8 ? 'active' : ''}`}
                                        type="button"
                                        aria-pressed={currentShiftValue >= 8}
                                        onClick={() => setCurrentShiftValue((value) => (value >= 8 ? 0 : 8))}
                                        onMouseEnter={openShiftPanel}
                                        onMouseLeave={closeShiftPanel}
                                >
                                        {currentBitshiftLabel}
                                </Button>
                        ) : null}
                        {showDebayerControl ? (
                                <Button className={`tool-chip tool-chip-round ${debayerEnabled ? 'active' : ''}`} type="button" aria-pressed={debayerEnabled} icon={bayerIcon} iconOnly={true} onClick={() => setDebayerEnabled((value) => !value)} />
                        ) : null}
                        <ResetViewButton onClick={onResetView} />

                        {viewerControlsOpen && showBitshiftControl ? (
                                <div className="viewer-shift-panel" onMouseEnter={openShiftPanel} onMouseLeave={closeShiftPanel}>
                                        <span className="viewer-shift-title">bitshift</span>
                                        <Slider
                                                min="0"
                                                max="8"
                                                step="1"
                                                value={currentShiftValue}
                                                aria-label="bitshift"
                                                aria-valuetext={currentBitshiftLabel}
                                                onChange={(event) => setCurrentShiftValue(Number(event.target.value))}
                                        />
                                </div>
                        ) : null}
                </div>
        );
}
