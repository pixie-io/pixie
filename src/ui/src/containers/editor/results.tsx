import './results.scss';

import {tablesFromResults} from 'components/chart/data';
import * as LineChart from 'components/chart/line-chart';
import * as Scatter from 'components/chart/scatter';
import {chartsFromSpec} from 'components/chart/spec';
import {Spinner} from 'components/spinner/spinner';
import {ExecuteQueryResult} from 'gql-types';
import * as React from 'react';
import {Nav, Tab} from 'react-bootstrap';

import {QueryResultErrors, QueryResultTable} from '../vizier/query-result-viewer';

interface ConsoleResultsProps {
  loading?: boolean;
  error?: string;
  data?: ExecuteQueryResult;
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
      if (!data || !data.ExecuteQuery) {
        return [];
      }
      if (data.ExecuteQuery.error.compilerError) {
        return [{ title: 'Errors', content: <QueryResultErrors errors={data.ExecuteQuery.error} /> }];
      }

      const tables = tablesFromResults(data.ExecuteQuery);

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

      return [...resultsTables, ...scatterPlot, ...lineCharts];
    }, [data]);

    if (tabs.length < 1 && !placeholder) {
      placeholder = <span>no results</span>;
    }

    return (
      <div className={`pixie-console-result${placeholder ? '-placeholder' : ''}`}>
        {placeholder || (
          <Tab.Container defaultActiveKey={0} id='query-results-tabs'>
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
      </div>
    );
  });
