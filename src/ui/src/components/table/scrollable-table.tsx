import * as React from 'react';
import { AutoSizer, Column, Table } from 'react-virtualized';
import 'react-virtualized/styles.css'; // Only needs to be imported once.

export interface TableColumnInfo {
    dataKey: string;
    label: string;
    key?: number;
    flexGrow?: number;  // The factor that this column width can grow by.
    width?: number;     // The width of the column.
    resizable?: boolean;
}

export interface ScrollableTableProps {
    data: any[];
    columnInfo: TableColumnInfo[];
}

/**
 * Scrollable table is a inifinite-scroll table component.
 * The width and height of the table is inherited from the enclosing element.
 */
export class ScrollableTable extends React.Component<ScrollableTableProps, {}> {
    render() {
        const data = this.props.data;
        const columnInfo = this.props.columnInfo;
        const renderTable = ({height, width}) => {
            return (<Table
                    width={width}
                    height={height}
                    headerHeight={20}
                    rowHeight={30}
                    rowCount={data ? data.length : 0}
                    rowGetter={({ index }) => data[index]}>
                    {columnInfo && columnInfo.map((colProp, idx) => {
                        if (!colProp.key) { colProp.key = idx; }
                        return (<Column {...colProp} />);
                    })}
                </Table>);
            };
        return (
            <AutoSizer>
                {renderTable}
            </AutoSizer>
        );
    }
}
