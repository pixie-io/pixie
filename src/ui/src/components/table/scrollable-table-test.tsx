import {mount} from 'enzyme';
import * as React from 'react';
import {ScrollableTable} from './scrollable-table';

function ExpandRenderer(rowData) {
  return (
    <div className='expanded'>{'Expanded row: ' + JSON.stringify(rowData)}</div>
  );
}

describe('<ScrollableTable/> test', () => {
  it('should have correct content', () => {
    const wrapper = mount(<div style={{ height: 150 }}><ScrollableTable
        data={[
          { col1: 1, col2: 'this is a string', col3: 100 },
          { col1: 2, col2: 'hello', col3: 200 },
          { col1: 3, col2: 'world', col3: 300 },
        ]}
        columnInfo={[
          { dataKey: 'col1', label: 'Col1', width: 60 },
          { dataKey: 'col2', label: 'Col2', width: 100 },
          { dataKey: 'col3', label: 'Col3', width: 60 },
        ]}
        height={300}
        width={300}
    /></div>);

    expect(wrapper.find('.ReactVirtualized__Table__headerColumn')).toHaveLength(3);
    expect(wrapper.find('.ReactVirtualized__Table__headerColumn').at(0).text()).toEqual('Col1');
    expect(wrapper.find('.ReactVirtualized__Table__headerColumn').at(1).text()).toEqual('Col2');
    expect(wrapper.find('.ReactVirtualized__Table__headerColumn').at(2).text()).toEqual('Col3');

    expect(wrapper.find('.scrollable-table--row-even')).toHaveLength(2);
    expect(wrapper.find('.scrollable-table--row-even').at(0).text()).toEqual('1this is a string100');
    expect(wrapper.find('.scrollable-table--row-even').at(1).text()).toEqual('3world300');

    expect(wrapper.find('.scrollable-table--row-odd')).toHaveLength(1);
    expect(wrapper.find('.scrollable-table--row-odd').at(0).text()).toEqual('2hello200');

    expect(wrapper.find('.scrollable-table--drag-handle')).toHaveLength(0);
  });

  it('should not show expand icons', () => {
    const wrapper = mount(<div style={{ height: 150 }}><ScrollableTable
        data={[
          { col1: 1, col2: 'this is a string', col3: 100 },
          { col1: 2, col2: 'hello', col3: 200 },
          { col1: 3, col2: 'world', col3: 300 },
        ]}
        columnInfo={[
          { dataKey: 'col1', label: 'Col1', width: 60 },
          { dataKey: 'col2', label: 'Col2', width: 100 },
          { dataKey: 'col3', label: 'Col3', width: 60 },
        ]}
        height={300}
        width={300}
    /></div>);

    expect(wrapper.find('img')).toHaveLength(0);
  });

  it('should show expand icons', () => {
    const wrapper = mount(<div style={{ height: 150 }}><ScrollableTable
        data={[
          { col1: 1, col2: 'this is a string', col3: 100 },
          { col1: 2, col2: 'hello', col3: 200 },
          { col1: 3, col2: 'world', col3: 300 },
        ]}
        columnInfo={[
          { dataKey: 'col1', label: 'Col1', width: 60 },
          { dataKey: 'col2', label: 'Col2', width: 100 },
          { dataKey: 'col3', label: 'Col3', width: 60 },
        ]}
        expandable={true}
        height={300}
        width={300}
  /></div>);

    expect(wrapper.find('img')).toHaveLength(3);
  });

  it('should show correct content when expanded', () => {
    const wrapper = mount(<div style={{ height: 150 }}><ScrollableTable
        data={[
          { col1: 1, col2: 'this is a string', col3: 100 },
          { col1: 2, col2: 'hello', col3: 200 },
          { col1: 3, col2: 'world', col3: 300 },
        ]}
        columnInfo={[
          { dataKey: 'col1', label: 'Col1', width: 60 },
          { dataKey: 'col2', label: 'Col2', width: 100 },
          { dataKey: 'col3', label: 'Col3', width: 60 },
        ]}
        expandable={true}
        expandRenderer={ExpandRenderer}
        height={300}
        width={300}
    /></div>);

    const row = wrapper.find('.ReactVirtualized__Table__row').at(1);
    row.simulate('click');

    expect(wrapper.find('.expanded')).toHaveLength(1);
    expect(wrapper.find('.expanded').text()).toEqual(
      'Expanded row: {\"col1\":1,\"col2\":\"this is a string\",\"col3\":100}');

    row.simulate('click');
    expect(wrapper.find('.expanded')).toHaveLength(0);
  });

  it('should show drag handle', () => {
    const wrapper = mount(<div style={{ height: 150 }}><ScrollableTable
        data={[
          { col1: 1, col2: 'this is a string', col3: 100 },
          { col1: 2, col2: 'hello', col3: 200 },
          { col1: 3, col2: 'world', col3: 300 },
        ]}
        columnInfo={[
          { dataKey: 'col1', label: 'Col1' },
          { dataKey: 'col2', label: 'Col2' },
          { dataKey: 'col3', label: 'Col3' },
        ]}
        height={300}
        width={300}
        resizableCols={true}
    /></div>);

    expect(wrapper.find('.ReactVirtualized__Table__headerColumn')).toHaveLength(3);
    expect(wrapper.find('.ReactVirtualized__Table__headerColumn').at(0).text()).toEqual('Col1|');
    expect(wrapper.find('.ReactVirtualized__Table__headerColumn').at(1).text()).toEqual('Col2|');
    expect(wrapper.find('.ReactVirtualized__Table__headerColumn').at(2).text()).toEqual('Col3');

    expect(wrapper.find('.scrollable-table--row-even')).toHaveLength(2);
    expect(wrapper.find('.scrollable-table--row-even').at(0).text()).toEqual('1this is a string100');
    expect(wrapper.find('.scrollable-table--row-even').at(1).text()).toEqual('3world300');

    expect(wrapper.find('.scrollable-table--row-odd')).toHaveLength(1);
    expect(wrapper.find('.scrollable-table--row-odd').at(0).text()).toEqual('2hello200');

    expect(wrapper.find('.scrollable-table--drag-handle')).toHaveLength(2);
  });

});
