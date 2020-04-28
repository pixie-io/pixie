import {VisualizationSpec} from 'vega-embed';

export interface VisualizationSpecMap {
  [key: string]: VisualizationSpec;
}

export function parseSpecs(spec: string): VisualizationSpecMap {
  try {
    const vega = JSON.parse(spec);
    return vega as VisualizationSpecMap;
  } catch (e) {
    return null;
  }
}

