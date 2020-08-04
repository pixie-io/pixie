import * as jspb from "google-protobuf"

import * as google_protobuf_any_pb from 'google-protobuf/google/protobuf/any_pb';

export class Vis extends jspb.Message {
  getVariablesList(): Array<Vis.Variable>;
  setVariablesList(value: Array<Vis.Variable>): void;
  clearVariablesList(): void;
  addVariables(value?: Vis.Variable, index?: number): Vis.Variable;

  getWidgetsList(): Array<Widget>;
  setWidgetsList(value: Array<Widget>): void;
  clearWidgetsList(): void;
  addWidgets(value?: Widget, index?: number): Widget;

  getGlobalFuncsList(): Array<Vis.GlobalFunc>;
  setGlobalFuncsList(value: Array<Vis.GlobalFunc>): void;
  clearGlobalFuncsList(): void;
  addGlobalFuncs(value?: Vis.GlobalFunc, index?: number): Vis.GlobalFunc;

  serializeBinary(): Uint8Array;
  toObject(includeInstance?: boolean): Vis.AsObject;
  static toObject(includeInstance: boolean, msg: Vis): Vis.AsObject;
  static serializeBinaryToWriter(message: Vis, writer: jspb.BinaryWriter): void;
  static deserializeBinary(bytes: Uint8Array): Vis;
  static deserializeBinaryFromReader(message: Vis, reader: jspb.BinaryReader): Vis;
}

export namespace Vis {
  export type AsObject = {
    variablesList: Array<Vis.Variable.AsObject>,
    widgetsList: Array<Widget.AsObject>,
    globalFuncsList: Array<Vis.GlobalFunc.AsObject>,
  }

  export class Variable extends jspb.Message {
    getName(): string;
    setName(value: string): void;

    getType(): PXType;
    setType(value: PXType): void;

    getDefaultValue(): string;
    setDefaultValue(value: string): void;

    getDescription(): string;
    setDescription(value: string): void;

    serializeBinary(): Uint8Array;
    toObject(includeInstance?: boolean): Variable.AsObject;
    static toObject(includeInstance: boolean, msg: Variable): Variable.AsObject;
    static serializeBinaryToWriter(message: Variable, writer: jspb.BinaryWriter): void;
    static deserializeBinary(bytes: Uint8Array): Variable;
    static deserializeBinaryFromReader(message: Variable, reader: jspb.BinaryReader): Variable;
  }

  export namespace Variable {
    export type AsObject = {
      name: string,
      type: PXType,
      defaultValue: string,
      description: string,
    }
  }


  export class GlobalFunc extends jspb.Message {
    getOutputName(): string;
    setOutputName(value: string): void;

    getFunc(): Widget.Func | undefined;
    setFunc(value?: Widget.Func): void;
    hasFunc(): boolean;
    clearFunc(): void;

    serializeBinary(): Uint8Array;
    toObject(includeInstance?: boolean): GlobalFunc.AsObject;
    static toObject(includeInstance: boolean, msg: GlobalFunc): GlobalFunc.AsObject;
    static serializeBinaryToWriter(message: GlobalFunc, writer: jspb.BinaryWriter): void;
    static deserializeBinary(bytes: Uint8Array): GlobalFunc;
    static deserializeBinaryFromReader(message: GlobalFunc, reader: jspb.BinaryReader): GlobalFunc;
  }

  export namespace GlobalFunc {
    export type AsObject = {
      outputName: string,
      func?: Widget.Func.AsObject,
    }
  }

}

export class Widget extends jspb.Message {
  getName(): string;
  setName(value: string): void;

  getPosition(): Widget.Position | undefined;
  setPosition(value?: Widget.Position): void;
  hasPosition(): boolean;
  clearPosition(): void;

  getFunc(): Widget.Func | undefined;
  setFunc(value?: Widget.Func): void;
  hasFunc(): boolean;
  clearFunc(): void;

  getGlobalFuncOutputName(): string;
  setGlobalFuncOutputName(value: string): void;

  getDisplaySpec(): google_protobuf_any_pb.Any | undefined;
  setDisplaySpec(value?: google_protobuf_any_pb.Any): void;
  hasDisplaySpec(): boolean;
  clearDisplaySpec(): void;

  getFuncOrRefCase(): Widget.FuncOrRefCase;

  serializeBinary(): Uint8Array;
  toObject(includeInstance?: boolean): Widget.AsObject;
  static toObject(includeInstance: boolean, msg: Widget): Widget.AsObject;
  static serializeBinaryToWriter(message: Widget, writer: jspb.BinaryWriter): void;
  static deserializeBinary(bytes: Uint8Array): Widget;
  static deserializeBinaryFromReader(message: Widget, reader: jspb.BinaryReader): Widget;
}

export namespace Widget {
  export type AsObject = {
    name: string,
    position?: Widget.Position.AsObject,
    func?: Widget.Func.AsObject,
    globalFuncOutputName: string,
    displaySpec?: google_protobuf_any_pb.Any.AsObject,
  }

  export class Position extends jspb.Message {
    getX(): number;
    setX(value: number): void;

    getY(): number;
    setY(value: number): void;

    getW(): number;
    setW(value: number): void;

    getH(): number;
    setH(value: number): void;

    serializeBinary(): Uint8Array;
    toObject(includeInstance?: boolean): Position.AsObject;
    static toObject(includeInstance: boolean, msg: Position): Position.AsObject;
    static serializeBinaryToWriter(message: Position, writer: jspb.BinaryWriter): void;
    static deserializeBinary(bytes: Uint8Array): Position;
    static deserializeBinaryFromReader(message: Position, reader: jspb.BinaryReader): Position;
  }

  export namespace Position {
    export type AsObject = {
      x: number,
      y: number,
      w: number,
      h: number,
    }
  }


  export class Func extends jspb.Message {
    getName(): string;
    setName(value: string): void;

    getArgsList(): Array<Widget.Func.FuncArg>;
    setArgsList(value: Array<Widget.Func.FuncArg>): void;
    clearArgsList(): void;
    addArgs(value?: Widget.Func.FuncArg, index?: number): Widget.Func.FuncArg;

    serializeBinary(): Uint8Array;
    toObject(includeInstance?: boolean): Func.AsObject;
    static toObject(includeInstance: boolean, msg: Func): Func.AsObject;
    static serializeBinaryToWriter(message: Func, writer: jspb.BinaryWriter): void;
    static deserializeBinary(bytes: Uint8Array): Func;
    static deserializeBinaryFromReader(message: Func, reader: jspb.BinaryReader): Func;
  }

  export namespace Func {
    export type AsObject = {
      name: string,
      argsList: Array<Widget.Func.FuncArg.AsObject>,
    }

    export class FuncArg extends jspb.Message {
      getName(): string;
      setName(value: string): void;

      getValue(): string;
      setValue(value: string): void;

      getVariable(): string;
      setVariable(value: string): void;

      getInputCase(): FuncArg.InputCase;

      serializeBinary(): Uint8Array;
      toObject(includeInstance?: boolean): FuncArg.AsObject;
      static toObject(includeInstance: boolean, msg: FuncArg): FuncArg.AsObject;
      static serializeBinaryToWriter(message: FuncArg, writer: jspb.BinaryWriter): void;
      static deserializeBinary(bytes: Uint8Array): FuncArg;
      static deserializeBinaryFromReader(message: FuncArg, reader: jspb.BinaryReader): FuncArg;
    }

    export namespace FuncArg {
      export type AsObject = {
        name: string,
        value: string,
        variable: string,
      }

      export enum InputCase { 
        INPUT_NOT_SET = 0,
        VALUE = 2,
        VARIABLE = 3,
      }
    }

  }


  export enum FuncOrRefCase { 
    FUNC_OR_REF_NOT_SET = 0,
    FUNC = 3,
    GLOBAL_FUNC_OUTPUT_NAME = 5,
  }
}

export class Axis extends jspb.Message {
  getLabel(): string;
  setLabel(value: string): void;

  serializeBinary(): Uint8Array;
  toObject(includeInstance?: boolean): Axis.AsObject;
  static toObject(includeInstance: boolean, msg: Axis): Axis.AsObject;
  static serializeBinaryToWriter(message: Axis, writer: jspb.BinaryWriter): void;
  static deserializeBinary(bytes: Uint8Array): Axis;
  static deserializeBinaryFromReader(message: Axis, reader: jspb.BinaryReader): Axis;
}

export namespace Axis {
  export type AsObject = {
    label: string,
  }
}

export class BarChart extends jspb.Message {
  getBar(): BarChart.Bar | undefined;
  setBar(value?: BarChart.Bar): void;
  hasBar(): boolean;
  clearBar(): void;

  getTitle(): string;
  setTitle(value: string): void;

  getXAxis(): Axis | undefined;
  setXAxis(value?: Axis): void;
  hasXAxis(): boolean;
  clearXAxis(): void;

  getYAxis(): Axis | undefined;
  setYAxis(value?: Axis): void;
  hasYAxis(): boolean;
  clearYAxis(): void;

  serializeBinary(): Uint8Array;
  toObject(includeInstance?: boolean): BarChart.AsObject;
  static toObject(includeInstance: boolean, msg: BarChart): BarChart.AsObject;
  static serializeBinaryToWriter(message: BarChart, writer: jspb.BinaryWriter): void;
  static deserializeBinary(bytes: Uint8Array): BarChart;
  static deserializeBinaryFromReader(message: BarChart, reader: jspb.BinaryReader): BarChart;
}

export namespace BarChart {
  export type AsObject = {
    bar?: BarChart.Bar.AsObject,
    title: string,
    xAxis?: Axis.AsObject,
    yAxis?: Axis.AsObject,
  }

  export class Bar extends jspb.Message {
    getValue(): string;
    setValue(value: string): void;

    getLabel(): string;
    setLabel(value: string): void;

    getStackBy(): string;
    setStackBy(value: string): void;

    getGroupBy(): string;
    setGroupBy(value: string): void;

    getHorizontal(): boolean;
    setHorizontal(value: boolean): void;

    serializeBinary(): Uint8Array;
    toObject(includeInstance?: boolean): Bar.AsObject;
    static toObject(includeInstance: boolean, msg: Bar): Bar.AsObject;
    static serializeBinaryToWriter(message: Bar, writer: jspb.BinaryWriter): void;
    static deserializeBinary(bytes: Uint8Array): Bar;
    static deserializeBinaryFromReader(message: Bar, reader: jspb.BinaryReader): Bar;
  }

  export namespace Bar {
    export type AsObject = {
      value: string,
      label: string,
      stackBy: string,
      groupBy: string,
      horizontal: boolean,
    }
  }

}

export class HistogramChart extends jspb.Message {
  getHistogram(): HistogramChart.Histogram | undefined;
  setHistogram(value?: HistogramChart.Histogram): void;
  hasHistogram(): boolean;
  clearHistogram(): void;

  getTitle(): string;
  setTitle(value: string): void;

  getXAxis(): Axis | undefined;
  setXAxis(value?: Axis): void;
  hasXAxis(): boolean;
  clearXAxis(): void;

  getYAxis(): Axis | undefined;
  setYAxis(value?: Axis): void;
  hasYAxis(): boolean;
  clearYAxis(): void;

  serializeBinary(): Uint8Array;
  toObject(includeInstance?: boolean): HistogramChart.AsObject;
  static toObject(includeInstance: boolean, msg: HistogramChart): HistogramChart.AsObject;
  static serializeBinaryToWriter(message: HistogramChart, writer: jspb.BinaryWriter): void;
  static deserializeBinary(bytes: Uint8Array): HistogramChart;
  static deserializeBinaryFromReader(message: HistogramChart, reader: jspb.BinaryReader): HistogramChart;
}

export namespace HistogramChart {
  export type AsObject = {
    histogram?: HistogramChart.Histogram.AsObject,
    title: string,
    xAxis?: Axis.AsObject,
    yAxis?: Axis.AsObject,
  }

  export class Histogram extends jspb.Message {
    getValue(): string;
    setValue(value: string): void;

    getMaxbins(): number;
    setMaxbins(value: number): void;

    getMinstep(): number;
    setMinstep(value: number): void;

    getHorizontal(): boolean;
    setHorizontal(value: boolean): void;

    getPrebinCount(): string;
    setPrebinCount(value: string): void;

    serializeBinary(): Uint8Array;
    toObject(includeInstance?: boolean): Histogram.AsObject;
    static toObject(includeInstance: boolean, msg: Histogram): Histogram.AsObject;
    static serializeBinaryToWriter(message: Histogram, writer: jspb.BinaryWriter): void;
    static deserializeBinary(bytes: Uint8Array): Histogram;
    static deserializeBinaryFromReader(message: Histogram, reader: jspb.BinaryReader): Histogram;
  }

  export namespace Histogram {
    export type AsObject = {
      value: string,
      maxbins: number,
      minstep: number,
      horizontal: boolean,
      prebinCount: string,
    }
  }

}

export class TimeseriesChart extends jspb.Message {
  getTimeseriesList(): Array<TimeseriesChart.Timeseries>;
  setTimeseriesList(value: Array<TimeseriesChart.Timeseries>): void;
  clearTimeseriesList(): void;
  addTimeseries(value?: TimeseriesChart.Timeseries, index?: number): TimeseriesChart.Timeseries;

  getTitle(): string;
  setTitle(value: string): void;

  getXAxis(): Axis | undefined;
  setXAxis(value?: Axis): void;
  hasXAxis(): boolean;
  clearXAxis(): void;

  getYAxis(): Axis | undefined;
  setYAxis(value?: Axis): void;
  hasYAxis(): boolean;
  clearYAxis(): void;

  serializeBinary(): Uint8Array;
  toObject(includeInstance?: boolean): TimeseriesChart.AsObject;
  static toObject(includeInstance: boolean, msg: TimeseriesChart): TimeseriesChart.AsObject;
  static serializeBinaryToWriter(message: TimeseriesChart, writer: jspb.BinaryWriter): void;
  static deserializeBinary(bytes: Uint8Array): TimeseriesChart;
  static deserializeBinaryFromReader(message: TimeseriesChart, reader: jspb.BinaryReader): TimeseriesChart;
}

export namespace TimeseriesChart {
  export type AsObject = {
    timeseriesList: Array<TimeseriesChart.Timeseries.AsObject>,
    title: string,
    xAxis?: Axis.AsObject,
    yAxis?: Axis.AsObject,
  }

  export class Timeseries extends jspb.Message {
    getValue(): string;
    setValue(value: string): void;

    getSeries(): string;
    setSeries(value: string): void;

    getStackBySeries(): boolean;
    setStackBySeries(value: boolean): void;

    getMode(): TimeseriesChart.Timeseries.Mode;
    setMode(value: TimeseriesChart.Timeseries.Mode): void;

    serializeBinary(): Uint8Array;
    toObject(includeInstance?: boolean): Timeseries.AsObject;
    static toObject(includeInstance: boolean, msg: Timeseries): Timeseries.AsObject;
    static serializeBinaryToWriter(message: Timeseries, writer: jspb.BinaryWriter): void;
    static deserializeBinary(bytes: Uint8Array): Timeseries;
    static deserializeBinaryFromReader(message: Timeseries, reader: jspb.BinaryReader): Timeseries;
  }

  export namespace Timeseries {
    export type AsObject = {
      value: string,
      series: string,
      stackBySeries: boolean,
      mode: TimeseriesChart.Timeseries.Mode,
    }

    export enum Mode { 
      MODE_UNKNOWN = 0,
      MODE_LINE = 2,
      MODE_POINT = 3,
      MODE_AREA = 4,
    }
  }

}

export class VegaChart extends jspb.Message {
  getSpec(): string;
  setSpec(value: string): void;

  serializeBinary(): Uint8Array;
  toObject(includeInstance?: boolean): VegaChart.AsObject;
  static toObject(includeInstance: boolean, msg: VegaChart): VegaChart.AsObject;
  static serializeBinaryToWriter(message: VegaChart, writer: jspb.BinaryWriter): void;
  static deserializeBinary(bytes: Uint8Array): VegaChart;
  static deserializeBinaryFromReader(message: VegaChart, reader: jspb.BinaryReader): VegaChart;
}

export namespace VegaChart {
  export type AsObject = {
    spec: string,
  }
}

export class Table extends jspb.Message {
  getDisplayColumnsList(): Array<Table.ColumnDisplay>;
  setDisplayColumnsList(value: Array<Table.ColumnDisplay>): void;
  clearDisplayColumnsList(): void;
  addDisplayColumns(value?: Table.ColumnDisplay, index?: number): Table.ColumnDisplay;

  serializeBinary(): Uint8Array;
  toObject(includeInstance?: boolean): Table.AsObject;
  static toObject(includeInstance: boolean, msg: Table): Table.AsObject;
  static serializeBinaryToWriter(message: Table, writer: jspb.BinaryWriter): void;
  static deserializeBinary(bytes: Uint8Array): Table;
  static deserializeBinaryFromReader(message: Table, reader: jspb.BinaryReader): Table;
}

export namespace Table {
  export type AsObject = {
    displayColumnsList: Array<Table.ColumnDisplay.AsObject>,
  }

  export class ColumnDisplay extends jspb.Message {
    getName(): string;
    setName(value: string): void;

    getDisplayTitle(): string;
    setDisplayTitle(value: string): void;

    getColumnCase(): ColumnDisplay.ColumnCase;

    serializeBinary(): Uint8Array;
    toObject(includeInstance?: boolean): ColumnDisplay.AsObject;
    static toObject(includeInstance: boolean, msg: ColumnDisplay): ColumnDisplay.AsObject;
    static serializeBinaryToWriter(message: ColumnDisplay, writer: jspb.BinaryWriter): void;
    static deserializeBinary(bytes: Uint8Array): ColumnDisplay;
    static deserializeBinaryFromReader(message: ColumnDisplay, reader: jspb.BinaryReader): ColumnDisplay;
  }

  export namespace ColumnDisplay {
    export type AsObject = {
      name: string,
      displayTitle: string,
    }

    export enum ColumnCase { 
      COLUMN_NOT_SET = 0,
      NAME = 1,
    }
  }

}

export class Graph extends jspb.Message {
  getDotColumn(): string;
  setDotColumn(value: string): void;

  getAdjacencyList(): Graph.AdjacencyList | undefined;
  setAdjacencyList(value?: Graph.AdjacencyList): void;
  hasAdjacencyList(): boolean;
  clearAdjacencyList(): void;

  getInputCase(): Graph.InputCase;

  serializeBinary(): Uint8Array;
  toObject(includeInstance?: boolean): Graph.AsObject;
  static toObject(includeInstance: boolean, msg: Graph): Graph.AsObject;
  static serializeBinaryToWriter(message: Graph, writer: jspb.BinaryWriter): void;
  static deserializeBinary(bytes: Uint8Array): Graph;
  static deserializeBinaryFromReader(message: Graph, reader: jspb.BinaryReader): Graph;
}

export namespace Graph {
  export type AsObject = {
    dotColumn: string,
    adjacencyList?: Graph.AdjacencyList.AsObject,
  }

  export class AdjacencyList extends jspb.Message {
    getFromColumn(): string;
    setFromColumn(value: string): void;

    getToColumn(): string;
    setToColumn(value: string): void;

    serializeBinary(): Uint8Array;
    toObject(includeInstance?: boolean): AdjacencyList.AsObject;
    static toObject(includeInstance: boolean, msg: AdjacencyList): AdjacencyList.AsObject;
    static serializeBinaryToWriter(message: AdjacencyList, writer: jspb.BinaryWriter): void;
    static deserializeBinary(bytes: Uint8Array): AdjacencyList;
    static deserializeBinaryFromReader(message: AdjacencyList, reader: jspb.BinaryReader): AdjacencyList;
  }

  export namespace AdjacencyList {
    export type AsObject = {
      fromColumn: string,
      toColumn: string,
    }
  }


  export enum InputCase { 
    INPUT_NOT_SET = 0,
    DOT_COLUMN = 1,
    ADJACENCY_LIST = 2,
  }
}

export class RequestGraph extends jspb.Message {
  getRequestorPodColumn(): string;
  setRequestorPodColumn(value: string): void;

  getResponderPodColumn(): string;
  setResponderPodColumn(value: string): void;

  getRequestorServiceColumn(): string;
  setRequestorServiceColumn(value: string): void;

  getResponderServiceColumn(): string;
  setResponderServiceColumn(value: string): void;

  getP50Column(): string;
  setP50Column(value: string): void;

  getP90Column(): string;
  setP90Column(value: string): void;

  getP99Column(): string;
  setP99Column(value: string): void;

  getErrorRateColumn(): string;
  setErrorRateColumn(value: string): void;

  getRequestsPerSecondColumn(): string;
  setRequestsPerSecondColumn(value: string): void;

  getInboundBytesPerSecondColumn(): string;
  setInboundBytesPerSecondColumn(value: string): void;

  getOutboundBytesPerSecondColumn(): string;
  setOutboundBytesPerSecondColumn(value: string): void;

  getTotalRequestCountColumn(): string;
  setTotalRequestCountColumn(value: string): void;

  serializeBinary(): Uint8Array;
  toObject(includeInstance?: boolean): RequestGraph.AsObject;
  static toObject(includeInstance: boolean, msg: RequestGraph): RequestGraph.AsObject;
  static serializeBinaryToWriter(message: RequestGraph, writer: jspb.BinaryWriter): void;
  static deserializeBinary(bytes: Uint8Array): RequestGraph;
  static deserializeBinaryFromReader(message: RequestGraph, reader: jspb.BinaryReader): RequestGraph;
}

export namespace RequestGraph {
  export type AsObject = {
    requestorPodColumn: string,
    responderPodColumn: string,
    requestorServiceColumn: string,
    responderServiceColumn: string,
    p50Column: string,
    p90Column: string,
    p99Column: string,
    errorRateColumn: string,
    requestsPerSecondColumn: string,
    inboundBytesPerSecondColumn: string,
    outboundBytesPerSecondColumn: string,
    totalRequestCountColumn: string,
  }
}

export enum PXType { 
  PX_UNKNOWN = 0,
  PX_BOOLEAN = 1,
  PX_INT64 = 2,
  PX_FLOAT64 = 3,
  PX_STRING = 4,
  PX_SERVICE = 1000,
  PX_POD = 1001,
  PX_CONTAINER = 1002,
  PX_NAMESPACE = 1003,
  PX_NODE = 1004,
}
