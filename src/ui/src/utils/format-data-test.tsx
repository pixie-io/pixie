import {mount, shallow} from 'enzyme';
import * as React from 'react';

import * as FormatData from './format-data';

describe('looksLikeLatencyCol test', () => {
  it('should not accept non-float latency columns', () => {
    expect(FormatData.looksLikeLatencyCol('latency', 'STRING')).toEqual(false);
  });

  it('should not accept incorrectly named columns', () => {
    expect(FormatData.looksLikeLatencyCol('CPU', 'FLOAT64')).toEqual(false);
  });

  it('should accept FLOAT64 columns with correct naming', () => {
    expect(FormatData.looksLikeLatencyCol('latency', 'FLOAT64')).toEqual(true);
  });
});

describe('looksLikeAlertCol test', () => {
  it('should not accept non-boolean alert columns', () => {
    expect(FormatData.looksLikeAlertCol('alert', 'STRING')).toEqual(false);
  });

  it('should not accept incorrectly named columns', () => {
    expect(FormatData.looksLikeAlertCol('CPU', 'BOOLEAN')).toEqual(false);
  });

  it('should accept BOOLEAN columns with correct naming', () => {
    expect(FormatData.looksLikeAlertCol('alert', 'BOOLEAN')).toEqual(true);
  });
});

describe('<LatencyData/> test', () => {
  it('should render correctly for low latency', () => {
    const wrapper = shallow(FormatData.LatencyData('20'));

    expect(wrapper.find('div').hasClass('formatted_data--latency-low')).toEqual(true);
    expect(wrapper.find('div').text()).toEqual('20');
  });

  it('should render correctly for medium latency', () => {
    const wrapper = shallow(FormatData.LatencyData('160'));

    expect(wrapper.find('div').hasClass('formatted_data--latency-med')).toEqual(true);
    expect(wrapper.find('div').text()).toEqual('160');
  });

  it('should render correctly for high latency', () => {
    const wrapper = shallow(FormatData.LatencyData('350'));

    expect(wrapper.find('div').hasClass('formatted_data--latency-high')).toEqual(true);
    expect(wrapper.find('div').text()).toEqual('350');
  });
});

describe('<AlertData/> test', () => {
  it('should render correctly for true alert', () => {
    const wrapper = shallow(FormatData.AlertData('true'));

    expect(wrapper.find('div').hasClass('formatted_data--alert-true')).toEqual(true);
    expect(wrapper.find('div').text()).toEqual('true');
  });

  it('should render correctly for false alert', () => {
    const wrapper = shallow(FormatData.AlertData('false'));

    expect(wrapper.find('div').hasClass('formatted_data--alert-false')).toEqual(true);
    expect(wrapper.find('div').text()).toEqual('false');
  });
});

describe('<JSONData/> test', () => {
  it('should render correctly for single line', () => {
    const wrapper = mount(<FormatData.JSONData
      data={{
        testString: 'a',
        testNum: 10,
        testNull: null,
        testJSON: {
          hello: 'world',
        },
      }}
    />);

    expect(wrapper.text()).toEqual('{ testString: a, testNum: 10, testNull: null, testJSON: { hello: world } }');

    const topLevelJSONContents = wrapper.find('.formatted_data--json').at(0).children();
    expect(topLevelJSONContents.find('.formatted_data--json-key')).toHaveLength(5);
    expect(topLevelJSONContents.find('.formatted_data--json-number')).toHaveLength(1);
    expect(topLevelJSONContents.find('.formatted_data--json-null')).toHaveLength(1);
    expect(topLevelJSONContents.find('.formatted_data--json-string')).toHaveLength(2);
    expect(topLevelJSONContents.find('.formatted_data--json')).toHaveLength(1);

    const innerLevelJSONContents = topLevelJSONContents.find('.formatted_data--json').at(0).children();
    expect(innerLevelJSONContents.find('.formatted_data--json-string')).toHaveLength(1);
    expect(innerLevelJSONContents.find('.formatted_data--json-key')).toHaveLength(1);
  });

  it('should render correctly for multiline', () => {
    const wrapper = mount(<FormatData.JSONData
      data={{
        testString: 'a',
        testNum: 10,
        testNull: null,
        testJSON: {
          hello: 'world',
        },
      }}
      multiline={true}
    />);

    expect(wrapper.text()).toEqual('{ testString: a, testNum: 10, testNull: null, testJSON: { hello: world } }');

    const topLevelJSONContents = wrapper.find('.formatted_data--json').at(0).children();
    expect(topLevelJSONContents.find('.formatted_data--json-key')).toHaveLength(5);
    expect(topLevelJSONContents.find('.formatted_data--json-number')).toHaveLength(1);
    expect(topLevelJSONContents.find('.formatted_data--json-null')).toHaveLength(1);
    expect(topLevelJSONContents.find('.formatted_data--json-string')).toHaveLength(2);
    expect(topLevelJSONContents.find('.formatted_data--json')).toHaveLength(1);
    expect(topLevelJSONContents.find('br')).toHaveLength(7);

    const innerLevelJSONContents = topLevelJSONContents.find('.formatted_data--json').at(0).children();
    expect(innerLevelJSONContents.find('.formatted_data--json-string')).toHaveLength(1);
    expect(innerLevelJSONContents.find('.formatted_data--json-key')).toHaveLength(1);

    expect(wrapper.find(FormatData.JSONData).at(1).props().multiline).toEqual(true);
    expect(wrapper.find(FormatData.JSONData).at(1).props().indentation).toEqual(1);
  });

  it('should render array correctly for single line', () => {
    const wrapper = mount(<FormatData.JSONData
      data={['some text', 'some other text']}
    />);

    expect(wrapper.text()).toEqual('[ some text, some other text ]');
    expect(wrapper.find('br')).toHaveLength(0);
  });

  it('should render array correctly for multiline', () => {
    const wrapper = mount(<FormatData.JSONData
      data={[
        { a: 1, b: { c: 'foo' } },
        { a: 3, b: null },
      ]}
      multiline={true}
    />);

    expect(wrapper.text()).toEqual('[ { a: 1, b: { c: foo } }, { a: 3, b: null } ]');
    expect(wrapper.find('br')).toHaveLength(11);
  });
});

describe('formatFloat64Data test', () => {
  it('should accept decimal-rendered scientific notation', () => {
    // 1e-6 renders to "0.000001" in numeral.js internally
    expect(FormatData.formatFloat64Data(1e-6)).toEqual('0');
  });
  it('should accept scientific notation rendered scientific notation', () => {
    // 1e-6 renders to '1e-7' internally in numeral.js, usually throws NaNs
    expect(FormatData.formatFloat64Data(1e-7)).toEqual('0');
  });
  it('should render NaNs correctly', () => {
    expect(FormatData.formatFloat64Data(NaN)).toEqual('NaN');
  });
  it('should render normal floats correctly', () => {
    expect(FormatData.formatFloat64Data(123.456)).toEqual('123.46');
  });
  it('should render huge floats correctly', () => {
    expect(FormatData.formatFloat64Data(123.456789101)).toEqual('123.46');
  });
});

describe('formatUint128 test', () => {
  it('should format to an uuid string', () => {
    expect(FormatData.formatUInt128('77311094061', '34858981')).toEqual('00000012-0019-ad2d-0000-00000213e7e5');
  });
});
