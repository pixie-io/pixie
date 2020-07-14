import { MuiThemeProvider } from '@material-ui/core';
import { DARK_THEME } from 'common/mui-theme';
import { mount, shallow } from 'enzyme';
import * as React from 'react';
import { AlertData, JSONData, LatencyData } from './format-data';

// TODO(nserrino): Fix these tests to follow JSS style guidelines.
xdescribe('<LatencyData/> test', () => {
  it('should render correctly for low latency', () => {
    const wrapper = shallow(<LatencyData data='20' />);

    expect(wrapper.find('div').prop('className')).toEqual('makeStyles-low-1');
    expect(wrapper.find('div').text()).toEqual('20');
  });

  it('should render correctly for medium latency', () => {
    const wrapper = shallow(<LatencyData data='160' />);

    expect(wrapper.find('div').prop('className')).toEqual('makeStyles-med-2');
    expect(wrapper.find('div').text()).toEqual('160');
  });

  it('should render correctly for high latency', () => {
    const wrapper = shallow(<LatencyData data='350' />);

    expect(wrapper.find('div').prop('className')).toEqual('makeStyles-high-3');
    expect(wrapper.find('div').text()).toEqual('350');
  });
});

xdescribe('<AlertData/> test', () => {
  it('should render correctly for true alert', () => {
    const wrapper = shallow(<AlertData data />);

    expect(wrapper.find('div').prop('className')).toEqual('makeStyles-true-4');
    expect(wrapper.find('div').text()).toEqual('true');
  });

  it('should render correctly for false alert', () => {
    const wrapper = shallow(<AlertData data={false} />);

    expect(wrapper.find('div').prop('className')).toEqual('makeStyles-false-5');
    expect(wrapper.find('div').text()).toEqual('false');
  });
});

xdescribe('<JSONData/> test', () => {
  it('should render correctly for single line', () => {
    const wrapper = mount(
      <MuiThemeProvider theme={DARK_THEME}>
        <JSONData
          data={{
            testString: 'a',
            testNum: 10,
            testNull: null,
            testJSON: {
              hello: 'world',
            },
          }}
        />
      </MuiThemeProvider>,
    );

    expect(wrapper.text()).toEqual('{ testString: a, testNum: 10, testNull: null, testJSON: { hello: world } }');
    const base = wrapper.find('span').at(0);
    expect(base.prop('className')).toEqual('makeStyles-base-6');
    const topLevelJSONContents = base.children();
    expect(topLevelJSONContents).toHaveLength(6);
    expect(topLevelJSONContents.find('.makeStyles-jsonKey-7')).toHaveLength(5);
    expect(topLevelJSONContents.find('.makeStyles-number-8')).toHaveLength(1);
    expect(topLevelJSONContents.find('.makeStyles-null-9')).toHaveLength(1);
    expect(topLevelJSONContents.find('.makeStyles-string-10')).toHaveLength(2);
    expect(topLevelJSONContents.find('.makeStyles-base-6')).toHaveLength(1);

    const innerLevelJSONContents = topLevelJSONContents.find('.makeStyles-base-6').at(0).children();
    expect(innerLevelJSONContents.find('.makeStyles-string-10')).toHaveLength(1);
    expect(innerLevelJSONContents.find('.makeStyles-jsonKey-7')).toHaveLength(1);
  });

  it('should render correctly for multiline', () => {
    const wrapper = mount(
      <MuiThemeProvider theme={DARK_THEME}>
        <JSONData
          data={{
            testString: 'a',
            testNum: 10,
            testNull: null,
            testJSON: {
              hello: 'world',
            },
          }}
          multiline
        />
      </MuiThemeProvider>,
    );

    expect(wrapper.text()).toEqual('{ testString: a, testNum: 10, testNull: null, testJSON: { hello: world } }');
    const base = wrapper.find('span').at(0);
    expect(base.prop('className')).toEqual('makeStyles-base-6');
    const topLevelJSONContents = base.children();

    expect(topLevelJSONContents.find('.makeStyles-jsonKey-7')).toHaveLength(5);
    expect(topLevelJSONContents.find('.makeStyles-number-8')).toHaveLength(1);
    expect(topLevelJSONContents.find('.makeStyles-null-9')).toHaveLength(1);
    expect(topLevelJSONContents.find('.makeStyles-string-10')).toHaveLength(2);
    expect(topLevelJSONContents.find('.makeStyles-base-6')).toHaveLength(1);
    expect(topLevelJSONContents.find('br')).toHaveLength(7);

    const innerLevelJSONContents = topLevelJSONContents.find('.makeStyles-base-6').at(0).children();
    expect(innerLevelJSONContents.find('.makeStyles-string-10')).toHaveLength(1);
    expect(innerLevelJSONContents.find('.makeStyles-jsonKey-7')).toHaveLength(1);

    expect(wrapper.find(JSONData).at(1).props().multiline).toEqual(true);
    expect(wrapper.find(JSONData).at(1).props().indentation).toEqual(1);
  });

  it('should render array correctly for single line', () => {
    const wrapper = mount(
      <MuiThemeProvider theme={DARK_THEME}>
        <JSONData
          data={['some text', 'some other text']}
        />
      </MuiThemeProvider>,
    );

    expect(wrapper.text()).toEqual('[ some text, some other text ]');
    expect(wrapper.find('br')).toHaveLength(0);
  });

  it('should render array correctly for multiline', () => {
    const wrapper = mount(
      <MuiThemeProvider theme={DARK_THEME}>
        <JSONData
          data={[
            { a: 1, b: { c: 'foo' } },
            { a: 3, b: null },
          ]}
          multiline
        />
      </MuiThemeProvider>,
    );

    expect(wrapper.text()).toEqual('[ { a: 1, b: { c: foo } }, { a: 3, b: null } ]');
    expect(wrapper.find('br')).toHaveLength(11);
  });
});
