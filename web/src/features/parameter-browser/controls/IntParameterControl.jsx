import React, { useEffect, useRef, useState } from 'react';
import Input from '../../../components/Input.jsx';
import Slider from '../../../components/Slider.jsx';
import {
        getIntParameterBounds,
        isLogarithmicIntParameter,
        logarithmicPositionToValue,
        LOGARITHMIC_SLIDER_STEPS,
        valueToLogarithmicPosition
} from './intParameterScale.js';

export default function IntParameterControl({ item, canEdit, onChange, parameterMeta }) {
        const [intDraftValue, setIntDraftValue] = useState(String(item.value ?? ''));
        const suppressNextBlurRef = useRef(false);
        const { minimum, maximum } = getIntParameterBounds(item);
        const useLogarithmicSlider = isLogarithmicIntParameter(item);
        const sliderValue = useLogarithmicSlider
                ? valueToLogarithmicPosition(item.value ?? minimum, minimum, maximum)
                : item.value ?? 0;

        useEffect(() => {
                setIntDraftValue(String(item.value ?? ''));
        }, [item.value, item.name]);

        function commitIntDraftValue() {
                return onChange(item.name, intDraftValue, { ...parameterMeta, interaction: 'number-commit' });
        }

        return (
                <div className="numeric-control">
                        <Slider
                                min={useLogarithmicSlider ? 0 : minimum}
                                max={useLogarithmicSlider ? LOGARITHMIC_SLIDER_STEPS : maximum}
                                step="1"
                                value={sliderValue}
                                disabled={!canEdit}
                                aria-valuetext={useLogarithmicSlider ? String(item.value ?? minimum) : undefined}
                                onChange={(event) => {
                                        const nextValue = useLogarithmicSlider
                                                ? String(logarithmicPositionToValue(event.target.value, minimum, maximum))
                                                : event.target.value;
                                        setIntDraftValue(nextValue);
                                        onChange(item.name, nextValue, { ...parameterMeta, interaction: 'slider' });
                                }}
                        />
                        <Input
                                type="number"
                                min={item.min ?? item.value ?? 0}
                                max={item.max ?? item.value ?? 100}
                                step="1"
                                value={intDraftValue}
                                disabled={!canEdit}
                                onChange={(event) => setIntDraftValue(event.target.value)}
                                onFocus={(event) => event.currentTarget.select()}
                                onBlur={() => {
                                        if (suppressNextBlurRef.current) {
                                                suppressNextBlurRef.current = false;
                                                return;
                                        }
                                        void commitIntDraftValue();
                                }}
                                onKeyDown={(event) => {
                                        if (event.key === 'Enter') {
                                                event.preventDefault();
                                                void commitIntDraftValue();
                                        }
                                }}
                                onKeyUp={(event) => {
                                        if (event.key === 'Enter') {
                                                suppressNextBlurRef.current = true;
                                                event.currentTarget.blur();
                                                event.currentTarget.focus({ preventScroll: true });
                                        }
                                }}
                        />
                </div>
        );
}
