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
        hasFrame,
        debayerEnabled,
        setDebayerEnabled,
        onResetView,
        status,
        frameCanvasRef,
        onCanvasMouseDown,
        frameMeta,
        displayedFrameNodeName,
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
                                hasFrame={hasFrame}
                                debayerEnabled={debayerEnabled}
                                setDebayerEnabled={setDebayerEnabled}
                                onResetView={onResetView}
                                status={status}
                                frameCanvasRef={frameCanvasRef}
                                onCanvasMouseDown={onCanvasMouseDown}
                                frameMeta={frameMeta}
                                displayedFrameNodeName={displayedFrameNodeName}
                                formatLabel={formatLabel}
                        />
                </section>
        );
}
