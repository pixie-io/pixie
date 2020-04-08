import {ChartDisplay} from './convert-to-vega-spec';
import {ChartPosition} from './layout';

// TODO(nserrino): Replace these with proto when the UI receives protobuf from the script manager
// instead of json from the json bundle.

interface FuncArg {
  name: string;
  value: string;
}

interface Func {
  name: string;
  args: FuncArg[];
}

interface Widget {
  name?: string;
  position: ChartPosition;
  func: Func;
  displaySpec: ChartDisplay | /* TableDisplay */ {};
}

export interface Vis {
  widgets: Widget[];
}

export function parseVis(json: string): Vis {
  try {
    const parsed = JSON.parse(json);
    if (typeof parsed === 'object') {
      return parsed as Vis;
    }
  } catch (e) {
    // noop. tslint doesn't allow empty blocks.
  }
  return null;
}
