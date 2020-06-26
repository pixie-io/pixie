import { Options } from 'vis-network/standalone';
import * as podSVG from './pod.svg';

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
  },
  physics: {
    solver: 'forceAtlas2Based',
    hierarchicalRepulsion: {
      nodeDistance: 100,
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
  },
};

export function getNamespaceFromEntityName(val: string): string {
  return val.split('/')[0];
}

export function getColorForLatency(val: number): string {
  return val < 100 ? LOW_EDGE_COLOR : (val > 200 ? HIGH_EDGE_COLOR : MED_EDGE_COLOR);
}

export function getColorForErrorRate(val: number): string {
  return val < 1 ? LOW_EDGE_COLOR : (val > 2 ? HIGH_EDGE_COLOR : MED_EDGE_COLOR);
}
