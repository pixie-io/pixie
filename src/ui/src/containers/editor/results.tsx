import './results.scss';

import {VizierQueryResult} from 'common/vizier-grpc-client';
import * as Graph from 'components/chart/graph';
import * as LineChart from 'components/chart/line-chart';
import * as Scatter from 'components/chart/scatter';
import {chartsFromSpec} from 'components/chart/spec';
import {Spinner} from 'components/spinner/spinner';
import {QueryResultErrors, QueryResultTable} from 'containers/vizier/query-result-viewer';
// @ts-ignore : TS does not like image files.
import * as gridViewIcon from 'images/icons/grid-view.svg';
import * as React from 'react';
import {Button, Modal, Nav, Tab} from 'react-bootstrap';
import {columnFromProto} from 'utils/result-data-utils';

interface ConsoleResultsProps {
  loading?: boolean;
  error?: string;
  data?: VizierQueryResult;
  code?: string;
}

interface ResultsTab {
  title: string;
  content: React.ReactNode;
  className?: string;
}

export const ConsoleResults = React.memo<ConsoleResultsProps>(
  ({ loading, error, data, code = '' }) => {
    let placeholder = null;
    if (loading) {
      placeholder = <Spinner />;
    } else if (!!error) {
      placeholder = <span>{error}</span>;
    }

    const tabs = React.useMemo<ResultsTab[]>(() => {
      if (!data) {
        return [];
      }
      if (data.status) {
        return [{ title: 'Errors', content: <QueryResultErrors status={data.status} /> }];
      }

      const tables = data.tables || [];
      const charts = chartsFromSpec(tables, code);
      if (charts.length > 0) {
        return charts.map((chart) => ({
          title: chart.title || 'Chart',
          content: chart.chart,
          className: 'pixie-console-result--chart',
        }));
      }

      const scatterPlotData = Scatter.parseData(tables);
      const scatterPlot = scatterPlotData ? [{
        title: `${tables[0].name || 'result'} plot`,
        content: <Scatter.ScatterPlot {...scatterPlotData} />,
        className: 'pixie-console-result--chart',
      }] : [];
      const lineCharts = [];
      for (const table of tables) {
        const lineSeriesData = LineChart.parseData(table);
        if (lineSeriesData.length > 0) {
          lineCharts.push({
            title: `${table.name || 'result'} timeseries`,
            content: <LineChart.LineChart lines={lineSeriesData} />,
            className: 'pixie-console-result--chart',
          });
        }
      }
      const resultsTables = tables.map((table) => ({
        title: table.name || 'result',
        content: <QueryResultTable data={table} />,
      }));
      const graphs = [];
      for (const table of tables) {
        if (table.name === '__query_plan__') {
          const rowBatches = table.data;
          if (rowBatches.length < 1) {
            continue;
          }
          const dotSpec = columnFromProto(rowBatches[0].getColsList()[0])[0];
          graphs.push({
            title: 'Query Plan',
            content: <Graph.Graph dot={dotSpec} />,
            className: '',
          });
        }
      }

      return [...resultsTables, ...scatterPlot, ...lineCharts, ...graphs];
    }, [data]);

    if (tabs.length < 1 && !placeholder) {
      placeholder = <span>no results</span>;
    }

    const [showGridView, setShowGridView] = React.useState(false);
    const openGridView = React.useCallback(() => setShowGridView(true), []);
    const closeGridView = React.useCallback(() => setShowGridView(false), []);
    const gridViewContent = React.useMemo(() => tabs.map((tab) => tab.content), [tabs]);

    return (
      <div className={`pixie-console-result${placeholder ? '-placeholder' : ''}`}>
        {placeholder || (
          <Tab.Container mountOnEnter={true} defaultActiveKey={0} id='query-results-tabs'>
            <Nav variant='tabs' className='pixie-console-result--tabs'>
              {tabs.map((tab, i) => (
                <Nav.Item
                  as={Nav.Link}
                  eventKey={i}
                  key={`tab-nav-${i}`}
                >
                  {tab.title}
                </Nav.Item>
              ))}
              <Button
                className='pixie-console-open-tile-view'
                onClick={openGridView}
                disabled={gridViewContent.length === 0}
              >
                <img src={gridViewIcon} />
              </Button>
            </Nav>
            <Tab.Content>
              {tabs.map((tab, i) => (
                <Tab.Pane
                  eventKey={i}
                  key={`tab-pane-${i}`}
                  className={tab.className || ''}
                >
                  {tab.content}
                </Tab.Pane>
              ))}
            </Tab.Content>
          </Tab.Container>
        )}
        <Modal show={showGridView} onHide={closeGridView} className='pixie-console-modal'>
          <ResultsGridView content={gridViewContent} />
        </Modal>
      </div>
    );
  });

ConsoleResults.displayName = 'ConsoleResults';

interface ResultsGridViewProps {
  content: React.ReactNode[];
}

const ResultsGridView = (props: ResultsGridViewProps) => {
  const styles = React.useMemo(() => {
    const columns = Math.round(Math.sqrt(props.content.length));
    return {
      gridTemplateColumns: `repeat(${columns}, auto)`,
    };
  }, [props.content]);
  return (<div className='pixie-console-results-grid-view' style={styles}>
    {props.content.map((content) => (<div>{content}</div>))}
  </div>);
};
