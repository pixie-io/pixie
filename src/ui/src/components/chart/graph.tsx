import './graph.scss';

import { WidgetDisplay } from 'containers/live/vis';
import * as d3 from 'd3';
import * as dagreD3 from 'dagre-d3';
import * as graphlibDot from 'graphlib-dot';
import * as React from 'react';

import { centerFit } from './graph-utils';

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

export const Graph = (props: GraphProps) => {
  const { dot, toCol, fromCol, data } = props;
  const [err, setErr] = React.useState('');
  const svgRef = React.useRef<SVGSVGElement>(null);
  const ref = React.useRef({
    svgGroup: null,
    renderer: null,
    zoom: null,
    baseSvg: null,
  });

  const dataToGraph = () => {
    const graph = new dagreD3.graphlib.Graph()
      .setGraph({})
      .setDefaultEdgeLabel(() => ({}));

    data.forEach((rb) => {
      // Filter out empty columns, because this will cause dagre to crash.
      if (toCol !== '' && fromCol !== '') {
        const nt = rb[toCol];
        const nf = rb[fromCol];
        if (!graph.hasNode(nt)) {
          graph.setNode(nt, {label: nt})
        }
        if (!graph.hasNode(nf)) {
          graph.setNode(nf, {label: nf})
        }

        graph.setEdge(nf, nt);
      }
    });
    return graph;
  };

  // Do this once to setup the component.
  React.useLayoutEffect(() => {
    const baseSvg = d3.select<SVGGraphicsElement, any>(svgRef.current);
    const svgGroup = baseSvg.append('g');
    const zoom = d3.zoom().on('zoom', () => svgGroup.attr('transform', d3.event.transform));
    const renderer = new dagreD3.render();
    baseSvg.call(zoom);
    ref.current = { svgGroup, baseSvg, zoom, renderer };
  }, []);

  React.useEffect(() => {
    const graph = dot ? graphlibDot.read(dot) : dataToGraph();
    const { baseSvg, svgGroup, renderer, zoom } = ref.current;
    try {
      renderer(svgGroup, graph);
      setErr('');
    } catch (error) {
      setErr('Error rendering graph. Graph may display incorrectly.');
    }
    // Center the graph
    const rootBbox = svgRef.current.getBoundingClientRect();
    const groupBbox = svgGroup.node().getBBox();

    baseSvg.call(zoom.transform, centerFit(rootBbox, groupBbox));
  }, [dot, data]);

  return (
    <div className='pixie-graph-root'>
      <svg className='pixie-graph-svg' ref={svgRef} />
      {err !== '' ? <p>{err}</p> : null}
    </div>
  );
};
