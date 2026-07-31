import React from 'react';

export default function FrameMetaOverlay({ nodeName, seqText, tsMs, captureText, renderText, frameMeta, formatLabel }) {
        const imageInfo = frameMeta
                ? `${formatLabel(frameMeta.formatId)}/${frameMeta.width}x${frameMeta.height}`
                : '-/-';

        return (
                <div className="frame-overlay-meta">
                        <div className="frame-overlay-node-name">{nodeName || '-'}</div>
                        <div>{imageInfo}</div>
                        <div>seq: {seqText} | ts: {tsMs} ms</div>
                        <div>capture fps: {captureText} | render fps: {renderText}</div>
                </div>
        );
}
