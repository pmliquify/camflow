import React from 'react';
import ControlHandle from './ControlHandle.jsx';

function clampProgress(value, minimum, maximum) {
        if (!Number.isFinite(value) || !Number.isFinite(minimum) || !Number.isFinite(maximum) || maximum <= minimum) {
                return 0;
        }
        return Math.min(100, Math.max(0, ((value - minimum) / (maximum - minimum)) * 100));
}

export default function Slider({ className = '', min = 0, max = 100, value = 0, reverse = false, style, ...props }) {
        const minimum = Number(min);
        const maximum = Number(max);
        const numericValue = Number(value);
        const progress = clampProgress(numericValue, minimum, maximum);
        const visualProgress = reverse ? 100 - progress : progress;
        const sliderClassName = `ui-slider${reverse ? ' ui-slider-reverse' : ''}${className ? ` ${className}` : ''}`;

        return (
                <span
                        className={sliderClassName}
                        style={{
                                ...style,
                                '--slider-progress': `${visualProgress}%`,
                                '--slider-handle-position': `calc(${visualProgress}% - ${(visualProgress * 20) / 100}px)`,
                        }}
                >
                        <span className="ui-slider-track" aria-hidden="true" />
                        <ControlHandle className="ui-slider-handle" />
                        <input {...props} className="ui-slider-input" type="range" min={min} max={max} value={value} />
                </span>
        );
}
