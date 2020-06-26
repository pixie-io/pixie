import { mount, shallow } from 'enzyme';
import * as React from 'react';
import { AlertData, JSONData, LatencyData } from './format-data';

describe('<LatencyData/> test', () => {
  it('should render correctly for low latency', () => {
    const wrapper = shallow(LatencyData('20'));

    expect(wrapper.find('div').hasClass('formatted_data--latency-low')).toEqual(true);
    expect(wrapper.find('div').text()).toEqual('20');
  });

  it('should render correctly for medium latency', () => {
    const wrapper = shallow(LatencyData('160'));

    expect(wrapper.find('div').hasClass('formatted_data--latency-med')).toEqual(true);
    expect(wrapper.find('div').text()).toEqual('160');
  });

  it('should render correctly for high latency', () => {
    const wrapper = shallow(LatencyData('350'));

    expect(wrapper.find('div').hasClass('formatted_data--latency-high')).toEqual(true);
    expect(wrapper.find('div').text()).toEqual('350');
  });
});

describe('<AlertData/> test', () => {
  it('should render correctly for true alert', () => {
    const wrapper = shallow(AlertData(true));

    expect(wrapper.find('div').hasClass('formatted_data--alert-true')).toEqual(true);
    expect(wrapper.find('div').text()).toEqual('true');
  });

  it('should render correctly for false alert', () => {
    const wrapper = shallow(AlertData(false));

    expect(wrapper.find('div').hasClass('formatted_data--alert-false')).toEqual(true);
    expect(wrapper.find('div').text()).toEqual('false');
  });
});

describe('<JSONData/> test', () => {
  it('should render correctly for single line', () => {
    const wrapper = mount(<JSONData
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
    const wrapper = mount(<JSONData
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

    expect(wrapper.find(JSONData).at(1).props().multiline).toEqual(true);
    expect(wrapper.find(JSONData).at(1).props().indentation).toEqual(1);
  });

  it('should render array correctly for single line', () => {
    const wrapper = mount(<JSONData
      data={['some text', 'some other text']}
    />);

    expect(wrapper.text()).toEqual('[ some text, some other text ]');
    expect(wrapper.find('br')).toHaveLength(0);
  });

  it('should render array correctly for multiline', () => {
    const wrapper = mount(<JSONData
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
