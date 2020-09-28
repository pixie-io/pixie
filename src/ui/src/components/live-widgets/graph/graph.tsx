import { WidgetDisplay } from 'containers/live/vis';
import {
  data as visData, Edge, Network, Node, parseDOTNetwork,
} from 'vis-network/standalone';
import * as React from 'react';
import {
  createStyles, makeStyles, useTheme, Theme,
} from '@material-ui/core/styles';
import { useHistory } from 'react-router';
import ClusterContext from 'common/cluster-context';
import { Arguments } from 'utils/args-utils';
import { DataType, Relation, SemanticType } from '../../../types/generated/vizier_pb';
import {
  getGraphOptions, semTypeToShapeConfig,
} from './graph-options';
import { toEntityURL, toSingleEntityPage } from '../utils/live-view-params';

interface ColInfo {
  type: DataType;
  semType: SemanticType;
  name: string;
}

interface AdjacencyList {
  toColumn: string;
  fromColumn: string;
}

interface EdgeThresholds {
  mediumThreshold: number;
  highThreshold: number;
}

export interface GraphDisplay extends WidgetDisplay {
  readonly dotColumn?: string;
  readonly adjacencyList?: AdjacencyList;
  readonly data?: any[];
  readonly edgeWeightColumn?: string;
  readonly nodeWeightColumn?: string;
  readonly edgeColorColumn?: string;
  readonly edgeThresholds?: EdgeThresholds;
  readonly edgeHoverInfo?: string[];
  readonly edgeLength?: number;
}

interface GraphWidgetProps {
  display: GraphDisplay;
  data: any[];
  relation: Relation;
  propagatedArgs?: Arguments;
}

const colInfoFromName = (relation: Relation, name: string): ColInfo => {
  const cols = relation.getColumnsList();
  for (let i = 0; i < cols.length; i++) {
    if (cols[i].getColumnName() === name) {
      return {
        name,
        type: cols[i].getColumnType(),
        semType: cols[i].getColumnSemanticType(),
      };
    }
  }
  return undefined;
};

export const GraphWidget = (props: GraphWidgetProps) => {
  const { display, data, relation } = props;
  if (display.dotColumn && data.length > 0) {
    return (
      <Graph dot={data[0][display.dotColumn]} />
    );
  } if (display.adjacencyList && display.adjacencyList.fromColumn && display.adjacencyList.toColumn) {
    const toColInfo = colInfoFromName(relation, display.adjacencyList.toColumn);
    const fromColInfo = colInfoFromName(relation, display.adjacencyList.fromColumn);
    if (toColInfo && fromColInfo) {
      return (
        <Graph
          data={data}
          toCol={toColInfo}
          fromCol={fromColInfo}
          propagatedArgs={props.propagatedArgs}
          {...display}
        />
      );
    }

    let errorMsg = `${display.adjacencyList.toColumn} and ${display.adjacencyList.fromColumn} columns do not exist`;
    if (toColInfo) {
      errorMsg = `${display.adjacencyList.fromColumn} column does not exist`;
    } else if (fromColInfo) {
      errorMsg = `${display.adjacencyList.toColumn} column does not exist`;
    }
    return <div>{errorMsg}</div>;
  }
  return <div key={props.display.dotColumn}>Invalid spec for graph</div>;
};

interface GraphProps {
  dot?: any;
  data?: any[];
  toCol?: ColInfo;
  fromCol?: ColInfo;
  propagatedArgs?: Arguments;
  edgeWeightColumn?: string;
  nodeWeightColumn?: string;
  edgeColorColumn?: string;
  edgeThresholds?: EdgeThresholds;
  edgeHoverInfo?: string[];
  edgeLength?: number;
}

const useStyles = makeStyles(() => createStyles({
  root: {
    width: '100%',
    height: '100%',
    display: 'flex',
    flexDirection: 'column',
    alignItems: 'flex-end',
    '&.focus': {
      border: '1px solid #353738',
    },
  },
  container: {
    width: '100%',
    height: '95%',
    '& > .vis-active': {
      boxShadow: 'none',
    },
  },
}));

interface GraphData {
  nodes: visData.DataSet<Node>;
  edges: visData.DataSet<Edge>;
  idToSemType: {[ key: string ]: SemanticType};
  propagatedArgs?: Arguments;
}

function getColorForEdge(val: number, theme: Theme, thresholds: EdgeThresholds): string {
  const medThreshold = thresholds ? thresholds.mediumThreshold : 100;
  const highThreshold = thresholds ? thresholds.highThreshold : 200;

  if (val < medThreshold) {
    return theme.palette.success.dark;
  }
  return val > highThreshold ? theme.palette.error.main : theme.palette.warning.main;
}

export const Graph = (props: GraphProps) => {
  const {
    dot, toCol, fromCol, data, propagatedArgs, edgeWeightColumn,
    nodeWeightColumn, edgeColorColumn, edgeThresholds, edgeHoverInfo, edgeLength,
  } = props;
  const theme = useTheme();
  const graphOpts = getGraphOptions(theme, edgeLength);

  // TODO(zasgar/michelle/nserrino): Remove the context information from here and elsewhere.
  const { selectedClusterName } = React.useContext(ClusterContext);
  const history = useHistory();

  const [focused, setFocused] = React.useState<boolean>(false);
  const toggleFocus = React.useCallback(() => setFocused((enabled) => !enabled), []);
  // eslint-disable-next-line @typescript-eslint/no-unused-vars
  const [network, setNetwork] = React.useState<Network>(null);
  const [graph, setGraph] = React.useState<GraphData>(null);
  const doubleClickCallback = React.useCallback((params?: any) => {
    if (params.nodes.length > 0) {
      const nodeID = params.nodes[0];
      const semType = graph.idToSemType[nodeID];
      if (semType === SemanticType.ST_SERVICE_NAME
        || semType === SemanticType.ST_POD_NAME
        || semType === SemanticType.ST_NAMESPACE_NAME) {
        const page = toSingleEntityPage(nodeID, semType, selectedClusterName);
        const pathname = toEntityURL(page, propagatedArgs);
        history.push(pathname);
      }
    }
  }, [graph, history, selectedClusterName]);

  const ref = React.useRef<HTMLDivElement>();

  // Load the graph.
  React.useEffect(() => {
    if (dot) {
      const dotData = parseDOTNetwork(dot);
      setGraph(dotData);
      return;
    }

    const edges = new visData.DataSet<Edge>();
    const nodes = new visData.DataSet<Node>();
    const idToSemType = {};

    const upsertNode = (label: string, st: SemanticType, weight: number) => {
      if (!idToSemType[label]) {
        const node = {
          ...semTypeToShapeConfig(st),
          id: label,
          label,
        };

        if (weight !== -1) {
          node.value = weight;
        }

        nodes.add(node);
        idToSemType[label] = st;
      }
    };
    data.forEach((d) => {
      const nt = d[toCol.name];
      const nf = d[fromCol.name];

      let nodeWeight = -1;
      if (nodeWeightColumn && nodeWeightColumn !== '') {
        nodeWeight = d[nodeWeightColumn];
      }

      upsertNode(nt, toCol.semType, nodeWeight);
      upsertNode(nf, fromCol.semType, nodeWeight);

      const edge = {
        from: nf,
        to: nt,
      } as Edge;

      if (edgeWeightColumn && edgeWeightColumn !== '') {
        edge.value = d[edgeWeightColumn];
      }

      if (edgeColorColumn) {
        edge.color = getColorForEdge(d[edgeColorColumn], theme, edgeThresholds);
      }

      if (edgeHoverInfo) {
        let edgeInfo = '';
        edgeHoverInfo.forEach((info, i) => {
          edgeInfo = `${edgeInfo}${i === 0 ? '' : '<br>'} ${info}: ${d[info]}`;
        });
        edge.title = edgeInfo;
      }

      edges.add(edge);
    });

    setGraph({
      nodes, edges, idToSemType,
    });
  }, [dot, data, toCol, fromCol]);

  // Load the data.
  React.useEffect(() => {
    if (!graph) {
      return;
    }
    const n = new Network(ref.current, graph, graphOpts);
    n.on('doubleClick', doubleClickCallback);
    setNetwork(n);
  }, [graph, doubleClickCallback]);

  const classes = useStyles();
  return (
    <div className={`${classes.root} ${focused ? 'focus' : ''}`} onFocus={toggleFocus} onBlur={toggleFocus}>
      <div className={classes.container} ref={ref} />
    </div>
  );
};
