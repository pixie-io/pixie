import 'react-virtualized/styles.css'; // Only needs to be imported once.
import './scrollable-table.scss';

import ModalTrigger from 'components/modal';
import * as expanded from 'images/icons/expanded.svg';
import * as unexpanded from 'images/icons/unexpanded.svg';
import * as _ from 'lodash';
import * as React from 'react';
import { DraggableCore } from 'react-draggable';
import {
    AutoSizer, Column, defaultTableRowRenderer, Table, TableRowProps,
} from 'react-virtualized';

import { IconButton } from '@material-ui/core';
import OpenInNewIcon from '@material-ui/icons/OpenInNew';

const ROW_HEIGHT = 40;
const EXPANDED_ROW_HEIGHT = 300;

export interface TableColumnInfo {
  dataKey: string;
  label: string;
  key?: number;
  flexGrow?: number;  // The factor that this column width can grow by.
  width: number;     // The width of the column.
  resizable?: boolean;
  type?: string;
}

export interface ScrollableTableProps {
  data: any[];
  columnInfo: TableColumnInfo[];
  expandable?: boolean;
  resizableCols?: boolean;
  expandRenderer?: (rowData: any) => JSX.Element;
  cellRenderer?: (cellData: any, columnInfo: TableColumnInfo) => React.ReactNode;
  width: number;
  height: number;
}

export type AutoSizedScrollableTableProps = Omit<ScrollableTableProps, 'width' | 'height'>;

export interface ExpandedRows {
  [key: number]: boolean;
}

export interface ScrollableTableState {
  expandedRows: ExpandedRows;
  table: any; // Ref to TableComponent.
  widths: number[];
}

function RowRenderer(props: TableRowProps) {
  let expandedContent = null;
  if (_.has(this.state.expandedRows, props.index)) {
    const content = this.props.expandRenderer(props.rowData);
    expandedContent = (
      <div className='scrollable-table--expanded'>
        <ModalTrigger
          trigger={<IconButton><OpenInNewIcon /></IconButton>}
          triggerClassName='scrollable-table--expanded-modal-trigger'
          content={content}
          contentClassName='scrollable-table--expanded-modal-content'
        />
        {content}
      </div>
    );
  }

  return (
    <div
      className={'scrollable-table--row-' + (props.index % 2 === 0 ? 'even' : 'odd')}
      key={'row-' + props.key}
      style={props.style}
    >
      {defaultTableRowRenderer({...props, key: '', style: null})}
      {expandedContent}
    </div>
  );
}

function CellRenderer(props) {
  const cellData = props.cellData;
  return (
    <div className='scrollable-table--cell'>
      {this.props.expandable && props.columnIndex === 0 ?
        <img src={_.has(this.state.expandedRows, props.rowIndex) ? expanded : unexpanded} /> : null
      }
      {this.props.cellRenderer ?
        this.props.cellRenderer(cellData, this.props.columnInfo[props.columnIndex]) : cellData}
    </div>
  );
}

export class ScrollableTable extends React.Component<ScrollableTableProps, ScrollableTableState> {
  constructor(props) {
    super(props);
    this.state = {
      expandedRows: {},
      table: React.createRef(),
      widths: [],
    };
  }

  expandRow(event) {
    const expandedRows = { ...this.state.expandedRows };
    if (typeof expandedRows[event.index] !== 'undefined') {
      delete expandedRows[event.index];
    } else {
      expandedRows[event.index] = true;
    }
    this.setState({ expandedRows });

    this.state.table.current.recomputeRowHeights();
    this.state.table.current.forceUpdate();
  }

  computeHeight(event) {
    return _.has(this.state.expandedRows, event.index) ? EXPANDED_ROW_HEIGHT : ROW_HEIGHT;
  }

  render() {
    const data = this.props.data;
    const columnInfo = this.props.columnInfo;
    return (<Table
      width={this.props.width}
      height={this.props.height}
      headerHeight={ROW_HEIGHT}
      onRowClick={this.props.expandable ? this.expandRow.bind(this) : null}
      ref={this.state.table}
      rowHeight={this.computeHeight.bind(this)}
      rowCount={data ? data.length : 0}
      rowGetter={({ index }) => data[index]}
      rowRenderer={RowRenderer.bind(this)}>
      {columnInfo && columnInfo.map((colProp, idx) => {
        if (!colProp.key) { colProp.key = idx; }

        // Calculate width of column.
        if (this.props.resizableCols) {
          if (this.state.widths.length === 0) {
            colProp.width = this.props.width / this.props.columnInfo.length;
          } else {
            colProp.width = this.state.widths[idx] * this.props.width;
          }
        }

        // If column is last column, don't render header with drag handle.
        if (idx === this.props.columnInfo.length - 1 || !this.props.resizableCols) {
          return (<Column
            key={colProp.key}
            cellRenderer={CellRenderer.bind(this)}
            {...colProp} />);
        }
        return (<Column
          key={colProp.key}
          headerRenderer={this.headerRenderer}
          cellRenderer={CellRenderer.bind(this)}
          {...colProp} />);
      })}
    </Table>);
  }

  headerRenderer = ({
    columnData,
    dataKey,
    disableSort,
    label,
    sortBy,
    sortDirection,
  }) => {
    return (
      <React.Fragment key={dataKey}>
        <div className='ReactVirtualized__Table__headerTruncatedText'>
          {label}
        </div>
        <DraggableCore
          onDrag={(event, { deltaX }) => {
            this.resizeRow({
              dataKey,
              deltaX,
            });
          }
          }
        >
          <span className='scrollable-table--drag-handle'>|</span>
        </DraggableCore>
      </React.Fragment>
    );
  }

  resizeRow = ({ dataKey, deltaX }) =>
    this.setState((prevState) => {
      let prevWidths = [];
      if (prevState.widths.length === 0 && this.props.width !== 0) {
        // If no widths defined, start with equal-sized columns.
        prevWidths = _.map(this.props.columnInfo, () => 1 / this.props.columnInfo.length);
      } else {
        prevWidths = prevState.widths;
      }

      // Find index of column being resized.
      const dataKeys = _.map(this.props.columnInfo, (col) => col.dataKey);
      const idx = _.findIndex(dataKeys, (key) => key === dataKey);

      const percentDelta = deltaX / this.props.width;
      prevWidths[idx] = prevWidths[idx] + percentDelta;
      prevWidths[idx + 1] = prevWidths[idx + 1] - percentDelta;

      return {
        widths: prevWidths,
      };
    })
}

/**
 * Scrollable table is a inifinite-scroll table component.
 * The width and height of the table is inherited from the enclosing element.
 */
export class AutoSizedScrollableTable extends React.Component<AutoSizedScrollableTableProps, {}> {
  render() {
    return (
      <AutoSizer>
        {({ height, width }) => {
          return <ScrollableTable
            height={height}
            width={width}
            {...this.props}
          />;
        }}
      </AutoSizer>
    );
  }
}
