import React from 'react';
import FrameControlsOverlay from './FrameControlsOverlay.jsx';
import FrameMetaOverlay from './FrameMetaOverlay.jsx';
import frameEmptyIcon from '../../assets/images/icon-frame-empty.svg';

export default function FrameViewport({
        frameViewportRef,
        frameAspectRatio,
        onWheelCapture,
        seqText,
        tsMs,
        captureText,
        renderText,
        frameMeta,
        displayedFrameNodeName,
        formatLabel,
        viewerControlsOpen,
        setViewerControlsOpen,
        hasFrame,
        currentShiftValue,
        setCurrentShiftValue,
        debayerEnabled,
        setDebayerEnabled,
        onResetView,
        status,
        frameCanvasRef,
        onCanvasMouseDown
}) {
        return (
                <div
                        className="frame-box"
                        ref={frameViewportRef}
                        style={{ aspectRatio: frameAspectRatio }}
                        onWheelCapture={onWheelCapture}
                >
                        {hasFrame ? (
                                <>
                                        <FrameMetaOverlay
                                                nodeName={displayedFrameNodeName}
                                                seqText={seqText}
                                                tsMs={tsMs}
                                                captureText={captureText}
                                                renderText={renderText}
                                                frameMeta={frameMeta}
                                                formatLabel={formatLabel}
                                        />

                                        <FrameControlsOverlay
                                                viewerControlsOpen={viewerControlsOpen}
                                                setViewerControlsOpen={setViewerControlsOpen}
                                                hasFrame={hasFrame}
                                                frameMeta={frameMeta}
                                                currentShiftValue={currentShiftValue}
                                                setCurrentShiftValue={setCurrentShiftValue}
                                                debayerEnabled={debayerEnabled}
                                                setDebayerEnabled={setDebayerEnabled}
                                                onResetView={onResetView}
                                        />
                                </>
                        ) : null}

                        {!hasFrame && status !== 'running' ? (
                                <div className="frame-empty-state">
                                        <img src={frameEmptyIcon} alt="" aria-hidden="true" className="frame-empty-icon" />
                                        <p>{status === 'stopped' ? 'PIPELINE STOPPED - CLICK START PIPELINE' : 'SERVICE OFFLINE'}</p>
                                </div>
                        ) : null}

                        <canvas ref={frameCanvasRef} onMouseDown={onCanvasMouseDown} onContextMenu={(event) => event.preventDefault()} />
                </div>
        );
}
