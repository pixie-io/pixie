import { Options } from 'vis-network/standalone';
import * as podSVG from './pod.svg';
import * as svcSVG from './svc.svg';
import { SemanticType } from '../../../types/generated/vizier_pb';

// TODO(michelle): Get these from MUI theme.
const HIGH_EDGE_COLOR = '#e54e5c';
const MED_EDGE_COLOR = '#dc9406';
const LOW_EDGE_COLOR = '#00bd8f';

export const LABEL_OPTIONS = {
  label: {
    min: 8,
    max: 20,
    maxVisible: 20,
  },
};

export const GRAPH_OPTIONS: Options = {
  clickToUse: true,
  layout: {
    randomSeed: 10,
    improvedLayout: false,
  },
  physics: {
    solver: 'forceAtlas2Based',
    forceAtlas2Based: {
      gravitationalConstant: -50,
    },
    hierarchicalRepulsion: {
      nodeDistance: 100,
    },
    stabilization: {
      iterations: 250,
      updateInterval: 10,
    },
  },
  edges: {
    smooth: false,
    scaling: {
      max: 5,
    },
    arrows: {
      to: {
        enabled: true,
        type: 'arrow',
        scaleFactor: 0.5,
      },
    },
  },
  nodes: {
    borderWidth: 0.5,
    scaling: LABEL_OPTIONS,
    font: {
      face: 'Roboto',
      color: '#a6a8ae',
      align: 'left',
    },
  },
};

const semTypeToIcon = {
  [SemanticType.ST_SERVICE_NAME]: svcSVG,
  [SemanticType.ST_POD_NAME]: podSVG,
};

export function semTypeToShapeConfig(st: SemanticType): any {
  if (semTypeToIcon[st]) {
    const icon = semTypeToIcon[st];
    return {
      shape: 'image',
      image: {
        selected: icon,
        unselected: icon,
      },
    };
  }
  return {
    shape: 'dot',
  };
}

export function getNamespaceFromEntityName(val: string): string {
  return val.split('/')[0];
}

export function getColorForLatency(val: number): string {
  if (val < 100) {
    return LOW_EDGE_COLOR;
  }
  return val > 200 ? HIGH_EDGE_COLOR : MED_EDGE_COLOR;
}

export function getColorForErrorRate(val: number): string {
  if (val < 1) {
    return LOW_EDGE_COLOR;
  }
  return val > 2 ? HIGH_EDGE_COLOR : MED_EDGE_COLOR;
}
