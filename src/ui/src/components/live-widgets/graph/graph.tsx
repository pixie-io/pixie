import './graph.scss';

import { WidgetDisplay } from 'containers/live/vis';
import * as d3 from 'd3';
import * as dagreD3 from 'dagre-d3';
import * as graphlibDot from 'graphlib-dot';
import * as React from 'react';

interface AdjacencyList {
  toColumn: string;
  fromColumn: string;
}

export interface GraphDisplay extends WidgetDisplay {
  readonly dotColumn?: string;
  readonly adjacencyList?: AdjacencyList;
  readonly data?: any[];
}

interface GraphWidgetProps {
  display: GraphDisplay;
  data: any[];
}

export const GraphWidget = (props: GraphWidgetProps) => {
  const { display, data } = props;
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
  data?: any[];
  toCol?: string;
  fromCol?: string;
}

export function overflows(parent: DOMRect, child: DOMRect): boolean {
  return child.height > parent.height || child.width > parent.width;
}

export function scaleToFit(parent: DOMRect, child: DOMRect): number {
  if (!overflows(parent, child)) {
    // Don't scale the child if it doesn't overflow.
    return 1;
  }
  return Math.min(parent.height / child.height, parent.width / child.width);
}

export function fitDirection(parent: DOMRect, child: DOMRect): 'x' | 'y' {
  return parent.width / child.width > parent.height / child.height ? 'y' : 'x';
}

export function centerFit(parent: DOMRect, child: DOMRect): d3.ZoomTransform {
  if (!overflows(parent, child)) {
    return d3.zoomIdentity.translate(parent.width / 2 - child.width / 2, parent.height / 2 - child.height / 2);
  }

  const scale = scaleToFit(parent, child);
  const direction = fitDirection(parent, child);
  const translate = {
    x: direction === 'x' ? 0 : parent.width / 2 - child.width * scale / 2,
    y: direction === 'y' ? 0 : parent.height / 2 - child.height * scale / 2,
  };

  return d3.zoomIdentity.translate(translate.x, translate.y).scale(scale);
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
