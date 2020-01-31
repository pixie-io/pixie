import * as d3 from 'd3';
import * as dagreD3 from 'dagre-d3';
import * as dot from 'graphlib-dot';
import * as React from 'react';
import {AutoSizer} from 'react-virtualized';

import './graph.scss';

interface GraphProps {
  dot: string;
}

export class Graph extends React.Component<GraphProps, {}> {
  constructor(props) {
    super(props);
  }

  componentDidMount = () => {
    const graph = dot.read(this.props.dot);
    const render = new dagreD3.render();
    const svg = d3.select('#pixie-graph svg');
    const svgGroup = svg.append('g');
    render(svgGroup, graph);
    const bbox = svg.node().getBBox();
    svg.style('width', bbox.width)
      .style('height', bbox.height);
  }

  render() {
    return <AutoSizer>{({height, width}) => {
        return <div id='pixie-graph' style={{ height, width }}><svg /></div>;
      }
    }</AutoSizer>;
  }
}
