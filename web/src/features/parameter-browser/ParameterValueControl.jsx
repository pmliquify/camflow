import React from 'react';
import BoolParameterControl from './controls/BoolParameterControl.jsx';
import OptionParameterControl from './controls/OptionParameterControl.jsx';
import ButtonParameterControl from './controls/ButtonParameterControl.jsx';
import IntParameterControl from './controls/IntParameterControl.jsx';
import DoubleParameterControl from './controls/DoubleParameterControl.jsx';
import StringParameterControl from './controls/StringParameterControl.jsx';

export default function ParameterValueControl({ item, canEdit, onChange, parameterMeta }) {
        if (item.type === 'bool') {
                return <BoolParameterControl item={item} canEdit={canEdit} onChange={onChange} parameterMeta={parameterMeta} />;
        }
        if (item.type === 'option') {
                return <OptionParameterControl item={item} canEdit={canEdit} onChange={onChange} parameterMeta={parameterMeta} />;
        }
        if (item.type === 'button') {
                return <ButtonParameterControl item={item} canEdit={canEdit} onChange={onChange} parameterMeta={parameterMeta} />;
        }
        if (item.type === 'int') {
                return <IntParameterControl item={item} canEdit={canEdit} onChange={onChange} parameterMeta={parameterMeta} />;
        }
        if (item.type === 'double') {
                return <DoubleParameterControl item={item} canEdit={canEdit} onChange={onChange} parameterMeta={parameterMeta} />;
        }
        return <StringParameterControl item={item} canEdit={canEdit} onChange={onChange} parameterMeta={parameterMeta} />;
}
