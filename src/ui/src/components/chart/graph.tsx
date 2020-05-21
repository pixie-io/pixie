import './graph.scss';

import { WidgetDisplay } from 'containers/live/vis';
import * as d3 from 'd3';
import * as dagreD3 from 'dagre-d3';
import * as dot from 'graphlib-dot';
import * as React from 'react';
import { AutoSizer } from 'react-virtualized';

interface AdjacencyList {
  toColumn: string;
  fromColumn: string;
}

export interface GraphDisplay extends WidgetDisplay {
  readonly dotColumn?: string;
  readonly adjacencyList?: AdjacencyList;
  readonly data?: any;
}

export function displayToGraph(display: GraphDisplay, data) {
  if (display.dotColumn && data.length > 0) {
    return (
      <Graph dot={data[0][display.dotColumn]} />
    );
  } else if (display.adjacencyList && display.adjacencyList.fromColumn && display.adjacencyList.toColumn) {
    return (
      <Graph data={data} toCol={display.adjacencyList.toColumn} fromCol={display.adjacencyList.fromColumn} />
    );
  }
  return <div key={name}>Invalid spec for graph</div>;
}

interface GraphProps {
  dot?: any;
  data?: any;
  toCol?: string;
  fromCol?: string;
}

export class Graph extends React.Component<GraphProps, {}> {
  err: string;
  constructor(props) {
    super(props);
    this.err = '';
  }

  dataToGraph = () => {
    const graph = new dagreD3.graphlib.Graph()
      .setGraph({})
      .setDefaultEdgeLabel(() => ({}));

    this.props.data.forEach((rb) => {
      // Filter out empty columns, because this will cause dagre to crash.
      if (this.props.toCol !== '' && this.props.fromCol !== '') {
        graph.setNode(rb[this.props.toCol], { label: rb[this.props.toCol] });
        graph.setNode(rb[this.props.fromCol], { label: rb[this.props.fromCol] });
        graph.setEdge(rb[this.props.fromCol], rb[this.props.toCol]);
      }
    });

    return graph;
  }

  componentDidMount = () => {
    const graph = this.props.dot ? dot.read(this.props.dot) : this.dataToGraph();
    const render = new dagreD3.render();
    const svg = d3.select<SVGGraphicsElement, any>('#pixie-graph svg');
    const svgGroup = svg.append('g');
    try {
      render(svgGroup, graph);
    } catch (error) {
      this.err = 'Error rendering graph. Graph may display incorrectly.';
    }
    const bbox = svg.node().getBBox();
    svg.style('width', bbox.width)
      .style('height', bbox.height);
  }

  render() {
    return (
      <AutoSizer>{({ height, width }) => {
        return (<div id='pixie-graph' style={{ height, width }}>
          <svg />
          {this.err !== '' ? <p>{this.err}</p> : null}
        </div>);
      }}
      </AutoSizer>
    );
  }
}
