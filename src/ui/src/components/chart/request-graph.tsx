import * as React from 'react';
import { WidgetDisplay } from 'containers/live/vis';
import { Network, Options } from 'vis-network/standalone';
import { createStyles, makeStyles } from '@material-ui/core/styles';
import { toEntityPathname, toSingleEntityPage } from 'containers/live/utils/live-view-params';
import ClusterContext from 'common/cluster-context';
import { SemanticType } from 'types/generated/vizier_pb';
import Button from '@material-ui/core/Button';
import { useHistory } from 'react-router-dom';
import { formatFloat64Data } from 'utils/format-data';

import * as svcSVG from './svc.svg';
import * as podSVG from './pod.svg';

// TODO(michelle): Get these from MUI theme.
const HIGH_EDGE_COLOR = '#e54e5c';
const MED_EDGE_COLOR = '#dc9406';
const LOW_EDGE_COLOR = '#00bd8f';

export interface RequestGraphDisplay extends WidgetDisplay {
  readonly requestorPodColumn: string;
  readonly responderPodColumn: string;
  readonly requestorServiceColumn: string;
  readonly responderServiceColumn: string;
  readonly p50Column: string;
  readonly p90Column: string;
  readonly p99Column: string;
  readonly errorRateColumn: string;
  readonly requestsPerSecondColumn: string;
  readonly inboundBytesPerSecondColumn: string;
  readonly outboundBytesPerSecondColumn: string;
  readonly totalRequestCountColumn: string;
}

interface Entity {
  id: string;
  label: string;
  namespace?: string;
  service?: string;
  pod?: string;

  // Determines how to size this node.
  value?: number;
}

interface Edge {
  from: string;
  to: string;
  p50?: number;
  p90?: number;
  p99?: number;
  errorRate?: number;
  rps?: number;
  inboundBPS?: number;
  outputBPS?: number;

  // Determines the size of the edge.
  value?: number;
  title?: string;
  color?: string;

  // Edge attributes.
  arrows: any;
}

interface RequestGraphProps {
  display: RequestGraphDisplay;
  data: any[];
}
const useStyles = makeStyles(() =>
  createStyles({
    root: {
      width: '100%',
      height: '100%',
      display: 'flex',
      flexDirection: 'column',
      alignItems: 'flex-end',
      border: '1px solid #161616',
      '&.focus': {
        border: '1px solid #353738',
      }
    },
    container: {
      width: '100%',
      height: '95%',
      '& > .vis-active': {
        boxShadow: 'none',
      }
    },
  }),
);

function getNamespaceFromEntityName(val: string): string {
  return val.split('/')[0]
}

function getColorForLatency(val: number): string {
  return  val < 100 ? LOW_EDGE_COLOR: (val > 200 ? HIGH_EDGE_COLOR : MED_EDGE_COLOR);
}

function getColorForErrorRate(val: number): string {
  return val < 1 ? LOW_EDGE_COLOR : (val > 2 ? HIGH_EDGE_COLOR : MED_EDGE_COLOR);
}

const LABEL_OPTIONS = {
  label: {
    min:8,
    max:20,
    maxVisible: 20,
  }
};

const GRAPH_OPTIONS: Options = {
  clickToUse: true,
  layout: {
    randomSeed: 10,
  },
  physics: {
    solver: 'forceAtlas2Based',
    hierarchicalRepulsion: {
      nodeDistance: 100
    },
  },
  edges: {
    smooth: {
      enabled: true,
      type: 'dynamic',
      roundness: 1.0,
    },
    scaling: {
      max: 5,
    },
  },
  nodes: {
    borderWidth: 0.5,
    shape: 'image',
    scaling: LABEL_OPTIONS,
    font: {
      face: 'Roboto',
      color: '#a6a8ae',
      align: 'left',
    },
    image: {
      selected: podSVG,
      unselected: podSVG,
    },
  }
};

export const RequestGraphWidget = (props: RequestGraphProps) => {
  const { selectedClusterName } = React.useContext(ClusterContext);
  const history = useHistory();

  const ref = React.useRef<HTMLDivElement>()
  const data = props.data; //parseDOTNetwork(dot);
  const display = props.display;

  const [nodes, setNodes] = React.useState<Entity[]>([]);
  const [edges, setEdges] = React.useState<Edge[]>([]);
  const [svcs, setSvcs] = React.useState<string[]>([]);
  const [clusteredMode, setClusteredMode] = React.useState<boolean>(false);
  const [hierarchyEnabled, setHierarchyEnabled] = React.useState<boolean>(false);
  const [latencyEnabled, setLatencyEnabled] = React.useState<boolean>(false);
  const [focused, setFocused] = React.useState<boolean>(false);

  const toggleMode = React.useCallback(() => setClusteredMode((clustered) => !clustered), []);
  const toggleHierarchy = React.useCallback(() => setHierarchyEnabled((enabled) => !enabled), []);
  const toggleColor = React.useCallback(() => setLatencyEnabled((enabled) => !enabled), []);
  const toggleFocus = React.useCallback(() => setFocused((enabled) => !enabled), []);

  const updateGraph = (data: any[], display: RequestGraphDisplay) => {
    const entities = new Array<Entity>();
    const edges = new Array<Edge>();

    // Map of pod names -> Entity.
    const pods = {};
    const svcMap = {};
    const UpsertPod = (svc: string, pod: string, bps: number): Entity => {
      if (pods[pod]) {
        pods[pod].value += bps;
        return pods[pod];
      }
      const p = {
        id: pod,
        label: pod,
        namespace: getNamespaceFromEntityName(pod),
        service: svc,
        pod: pod,
        value: bps,
      }

      svcMap[svc] = true;

      pods[pod] = p;
      entities.push(p);

      return p;
    }

    data.forEach(value => {
      const req = UpsertPod(
        value[display.requestorServiceColumn],
        value[display.requestorPodColumn],
        value[display.outboundBytesPerSecondColumn]);
      const resp = UpsertPod(
        value[display.responderServiceColumn],
        value[display.responderPodColumn],
        value[display.inboundBytesPerSecondColumn]);

      const bps = value[display.inboundBytesPerSecondColumn] + value[display.outboundBytesPerSecondColumn];
      const rps = value[display.requestsPerSecondColumn];
      const errRate =  value[display.errorRateColumn];
      const latencyP50 = formatFloat64Data(value[display.p50Column]);
      const latencyP99 = formatFloat64Data(value[display.p99Column]);
      const title = `${formatFloat64Data(bps)} B/s <br />
                     ${formatFloat64Data(rps)} req/s <br >
                     Error: ${formatFloat64Data(errRate) + '%'} <br>
                     p50: ${latencyP50}s <br>
                     p99: ${latencyP99}s`;
      const latency = value[display.p99Column];

      edges.push({
        from: req.id,
        to: resp.id,
        p50: value[display.p50Column],
        p90: value[display.p90Column],
        p99: value[display.p99Column],
        errorRate: errRate,
        rps: rps,
        inboundBPS: value[display.inboundBytesPerSecondColumn],
        outputBPS: value[display.outboundBytesPerSecondColumn],
        value: bps,
        title: title,
        color: latencyEnabled ? getColorForLatency(latency) : getColorForErrorRate(errRate),
        arrows: {
          to: {
            enabled: true,
            type: 'arrow',
            scaleFactor: 0.5,
          },
        },
      });
    });

    setNodes(entities);
    setEdges(edges);
    setSvcs(Object.keys(svcMap));
  };

  React.useEffect(() => {
    updateGraph(data, display);
  }, [data, display, latencyEnabled]);

  React.useEffect(() => {
    const data = {
      nodes: nodes,
      edges: edges,
    };

    const options = GRAPH_OPTIONS;
    if (hierarchyEnabled) {
      options.layout.hierarchical= {
        enabled: true,
        levelSeparation: 200,
        direction: 'LR',
        sortMethod: 'directed',
      };
    } else {
      options.layout.hierarchical = null;
    }

    const network = new Network(ref.current as any, data, options);

    if (clusteredMode) {
      svcs.forEach((svc) => {
        const clusterOptionsByData = {
            joinCondition: function(childOptions) {
                return childOptions.service === svc;
            },
            clusterNodeProperties: {
              shape:'image',
              image: {
                selected: svcSVG,
                unselected: svcSVG,
              },
              allowSingleNodeCluster: true,
              label: svc,
              scaling: LABEL_OPTIONS,
            },
          processProperties: function (clusterOptions,
                                       childNodes) {
            let totalValue = 0;
            childNodes.forEach((node) => {
              totalValue += node.value;
            })
            clusterOptions.value = totalValue;
            clusterOptions.id = svc;

            return clusterOptions;
          },
        };
        network.cluster(clusterOptionsByData);
      })
    }

    network.on('doubleClick', function (params) {
      if (params.nodes.length > 0) {
        const semType = !clusteredMode ?  SemanticType.ST_POD_NAME : SemanticType.ST_SERVICE_NAME;
        const page = toSingleEntityPage(params.nodes[0], semType, selectedClusterName);
        const pathname = toEntityPathname(page);
         history.push(pathname);
      }
    })

  }, [ref, nodes, edges, svcs, clusteredMode, hierarchyEnabled, latencyEnabled])


  const classes = useStyles();
  return (
    <div className={classes.root + ' ' + (focused ? 'focus' : '')} onFocus={toggleFocus} onBlur={toggleFocus}>
      <div  className={classes.container} ref={ref} />
      <div>
        <Button
          size='small'
          onClick={toggleColor}>{latencyEnabled ? 'Color by error rate' : 'Color by latency'}
        </Button>
        <Button
          size='small'
          onClick={toggleHierarchy}>{hierarchyEnabled ? 'Disable hierarchy' : 'Enable hierarchy'}
        </Button>
        <Button
          size='small'
          onClick={toggleMode}>{clusteredMode ? 'Disable clustering' : 'Cluster by service'}
        </Button>
      </div>
    </div>);
};
