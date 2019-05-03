import * as _ from 'lodash';
import * as React from 'react';
import { AutoSizer, Column, defaultTableRowRenderer, Table } from 'react-virtualized';
import 'react-virtualized/styles.css'; // Only needs to be imported once.
import './scrollable-table.scss';

// @ts-ignore : TS does not like image files.
import * as expanded from 'images/icons/expanded.svg';
// @ts-ignore : TS does not like image files.
import * as unexpanded from 'images/icons/unexpanded.svg';

const ROW_HEIGHT = 40;
const EXPANDED_ROW_HEIGHT = 180;

export interface TableColumnInfo {
    dataKey: string;
    label: string;
    key?: number;
    flexGrow?: number;  // The factor that this column width can grow by.
    width?: number;     // The width of the column.
    resizable?: boolean;
    type?: string;
}

export interface AutoSizedScrollableTableProps {
    data: any[];
    columnInfo: TableColumnInfo[];
    expandable?: boolean;
    expandRenderer?: (rowData: any) => JSX.Element;
    cellRenderer?: (cellData: any, columnInfo: TableColumnInfo) => JSX.Element;
}

export interface ScrollableTableProps {
    data: any[];
    columnInfo: TableColumnInfo[];
    expandable?: boolean;
    expandRenderer?: (rowData: any) => JSX.Element;
    cellRenderer?: (cellData: any, columnInfo: TableColumnInfo) => JSX.Element;
    width: number;
    height: number;
}

export interface ExpandedRows {
  [key: number]: boolean;
}

export interface ScrollableTableState {
    expandedRows: ExpandedRows;
    table: any; // Ref to TableComponent.
}

function DefaultRowRenderer(props) {
    return defaultTableRowRenderer(props);
}

function RowRenderer(props) {
    const rowProps = _.omit(props, 'style', 'key');
    return (
        <div className={'scrollable-table--row-' + (props.index % 2 === 0 ? 'even' : 'odd')} style={props.style}>
            <DefaultRowRenderer {...rowProps}/>
             {_.has(this.state.expandedRows, props.index) ?
                <div className='scrollable-table--expanded'>{this.props.expandRenderer(props.rowData)}</div> : null}
        </div>
    );
}

function CellRenderer(props) {
  const cellData = props.cellData;
  return (
    <div className='scrollable-table--cell'>
      {this.props.expandable && props.columnIndex === 0 ?
        <img src={_.has(this.state.expandedRows, props.rowIndex) ? expanded : unexpanded}/> : null
      }
      { this.props.cellRenderer ?
          this.props.cellRenderer(cellData, this.props.columnInfo[props.columnIndex]) : cellData }
    </div>
  );
}

export class ScrollableTable extends React.Component<ScrollableTableProps, ScrollableTableState> {
    constructor(props) {
        super(props);
        this.state = {
            expandedRows: {},
            table: React.createRef(),
        };
    }

    expandRow(event) {
        if (_.has(this.state.expandedRows, event.index)) {
            _.unset(this.state.expandedRows, event.index);
        } else {
            this.state.expandedRows[event.index] = true;
        }
        this.setState({expandedRows: this.state.expandedRows});

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
                    return (<Column
                        cellRenderer={CellRenderer.bind(this)}
                        {...colProp} />);
                })}
            </Table>);
    }
}

/**
 * Scrollable table is a inifinite-scroll table component.
 * The width and height of the table is inherited from the enclosing element.
 */
export class AutoSizedScrollableTable extends React.Component<AutoSizedScrollableTableProps, {}> {
    render() {
        return (
            <AutoSizer>
              {({height, width}) => {
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
