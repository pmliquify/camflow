import React from 'react';
import FrameViewport from './FrameViewport.jsx';

export default function FrameViewerPanel({
        viewMode,
        frameViewportRef,
        frameAspectRatio,
        onWheelCapture,
        seqText,
        tsMs,
        captureText,
        renderText,
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
        onCanvasMouseDown,
        frameMeta,
        formatLabel
}) {
        return (
                <section className={`panel viewer-panel ${viewMode === 'viewer' ? 'foreground' : 'background'}`}>
                        <FrameViewport
                                frameViewportRef={frameViewportRef}
                                frameAspectRatio={frameAspectRatio}
                                onWheelCapture={onWheelCapture}
                                seqText={seqText}
                                tsMs={tsMs}
                                captureText={captureText}
                                renderText={renderText}
                                viewerControlsOpen={viewerControlsOpen}
                                setViewerControlsOpen={setViewerControlsOpen}
                                hasFrame={hasFrame}
                                currentShiftValue={currentShiftValue}
                                setCurrentShiftValue={setCurrentShiftValue}
                                debayerEnabled={debayerEnabled}
                                setDebayerEnabled={setDebayerEnabled}
                                onResetView={onResetView}
                                status={status}
                                frameCanvasRef={frameCanvasRef}
                                onCanvasMouseDown={onCanvasMouseDown}
                                frameMeta={frameMeta}
                                formatLabel={formatLabel}
                        />
                </section>
        );
}
