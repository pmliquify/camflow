export const LOGARITHMIC_RANGE_THRESHOLD = 100000;
export const LOGARITHMIC_SLIDER_STEPS = 1000;

function clamp(value, minimum, maximum) {
        return Math.min(maximum, Math.max(minimum, value));
}

export function getIntParameterBounds(item) {
        return {
                minimum: Number(item.min ?? item.value ?? 0),
                maximum: Number(item.max ?? item.value ?? 100)
        };
}

export function isLogarithmicIntParameter(item) {
        if (item?.type !== 'int') {
                return false;
        }

        const { minimum, maximum } = getIntParameterBounds(item);
        return maximum - minimum > LOGARITHMIC_RANGE_THRESHOLD;
}

export function valueToLogarithmicPosition(value, minimum, maximum) {
        const clampedValue = clamp(Number(value), minimum, maximum);
        return (Math.log1p(clampedValue - minimum) / Math.log1p(maximum - minimum)) * LOGARITHMIC_SLIDER_STEPS;
}

export function logarithmicPositionToValue(position, minimum, maximum) {
        const normalizedPosition = clamp(Number(position), 0, LOGARITHMIC_SLIDER_STEPS) / LOGARITHMIC_SLIDER_STEPS;
        const value = minimum + Math.expm1(normalizedPosition * Math.log1p(maximum - minimum));
        return clamp(Math.round(value), minimum, maximum);
}