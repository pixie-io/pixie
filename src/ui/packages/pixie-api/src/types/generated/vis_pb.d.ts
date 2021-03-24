import * as jspb from 'google-protobuf'

import * as google_protobuf_any_pb from 'google-protobuf/google/protobuf/any_pb';
import * as google_protobuf_wrappers_pb from 'google-protobuf/google/protobuf/wrappers_pb';


export class Vis extends jspb.Message {
  getVariablesList(): Array<Vis.Variable>;
  setVariablesList(value: Array<Vis.Variable>): Vis;
  clearVariablesList(): Vis;
  addVariables(value?: Vis.Variable, index?: number): Vis.Variable;

  getWidgetsList(): Array<Widget>;
  setWidgetsList(value: Array<Widget>): Vis;
  clearWidgetsList(): Vis;
  addWidgets(value?: Widget, index?: number): Widget;

  getGlobalFuncsList(): Array<Vis.GlobalFunc>;
  setGlobalFuncsList(value: Array<Vis.GlobalFunc>): Vis;
  clearGlobalFuncsList(): Vis;
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
    setName(value: string): Variable;

    getType(): PXType;
    setType(value: PXType): Variable;

    getDefaultValue(): google_protobuf_wrappers_pb.StringValue | undefined;
    setDefaultValue(value?: google_protobuf_wrappers_pb.StringValue): Variable;
    hasDefaultValue(): boolean;
    clearDefaultValue(): Variable;

    getDescription(): string;
    setDescription(value: string): Variable;

    getValidValuesList(): Array<string>;
    setValidValuesList(value: Array<string>): Variable;
    clearValidValuesList(): Variable;
    addValidValues(value: string, index?: number): Variable;

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
      defaultValue?: google_protobuf_wrappers_pb.StringValue.AsObject,
      description: string,
      validValuesList: Array<string>,
    }
  }


  export class GlobalFunc extends jspb.Message {
    getOutputName(): string;
    setOutputName(value: string): GlobalFunc;

    getFunc(): Widget.Func | undefined;
    setFunc(value?: Widget.Func): GlobalFunc;
    hasFunc(): boolean;
    clearFunc(): GlobalFunc;

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
  setName(value: string): Widget;

  getPosition(): Widget.Position | undefined;
  setPosition(value?: Widget.Position): Widget;
  hasPosition(): boolean;
  clearPosition(): Widget;

  getFunc(): Widget.Func | undefined;
  setFunc(value?: Widget.Func): Widget;
  hasFunc(): boolean;
  clearFunc(): Widget;

  getGlobalFuncOutputName(): string;
  setGlobalFuncOutputName(value: string): Widget;

  getDisplaySpec(): google_protobuf_any_pb.Any | undefined;
  setDisplaySpec(value?: google_protobuf_any_pb.Any): Widget;
  hasDisplaySpec(): boolean;
  clearDisplaySpec(): Widget;

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
    setX(value: number): Position;

    getY(): number;
    setY(value: number): Position;

    getW(): number;
    setW(value: number): Position;

    getH(): number;
    setH(value: number): Position;

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
    setName(value: string): Func;

    getArgsList(): Array<Widget.Func.FuncArg>;
    setArgsList(value: Array<Widget.Func.FuncArg>): Func;
    clearArgsList(): Func;
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
      setName(value: string): FuncArg;

      getValue(): string;
      setValue(value: string): FuncArg;

      getVariable(): string;
      setVariable(value: string): FuncArg;

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
  setLabel(value: string): Axis;

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
  setBar(value?: BarChart.Bar): BarChart;
  hasBar(): boolean;
  clearBar(): BarChart;

  getTitle(): string;
  setTitle(value: string): BarChart;

  getXAxis(): Axis | undefined;
  setXAxis(value?: Axis): BarChart;
  hasXAxis(): boolean;
  clearXAxis(): BarChart;

  getYAxis(): Axis | undefined;
  setYAxis(value?: Axis): BarChart;
  hasYAxis(): boolean;
  clearYAxis(): BarChart;

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
    setValue(value: string): Bar;

    getLabel(): string;
    setLabel(value: string): Bar;

    getStackBy(): string;
    setStackBy(value: string): Bar;

    getGroupBy(): string;
    setGroupBy(value: string): Bar;

    getHorizontal(): boolean;
    setHorizontal(value: boolean): Bar;

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
  setHistogram(value?: HistogramChart.Histogram): HistogramChart;
  hasHistogram(): boolean;
  clearHistogram(): HistogramChart;

  getTitle(): string;
  setTitle(value: string): HistogramChart;

  getXAxis(): Axis | undefined;
  setXAxis(value?: Axis): HistogramChart;
  hasXAxis(): boolean;
  clearXAxis(): HistogramChart;

  getYAxis(): Axis | undefined;
  setYAxis(value?: Axis): HistogramChart;
  hasYAxis(): boolean;
  clearYAxis(): HistogramChart;

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
    setValue(value: string): Histogram;

    getMaxbins(): number;
    setMaxbins(value: number): Histogram;

    getMinstep(): number;
    setMinstep(value: number): Histogram;

    getHorizontal(): boolean;
    setHorizontal(value: boolean): Histogram;

    getPrebinCount(): string;
    setPrebinCount(value: string): Histogram;

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
  setTimeseriesList(value: Array<TimeseriesChart.Timeseries>): TimeseriesChart;
  clearTimeseriesList(): TimeseriesChart;
  addTimeseries(value?: TimeseriesChart.Timeseries, index?: number): TimeseriesChart.Timeseries;

  getTitle(): string;
  setTitle(value: string): TimeseriesChart;

  getXAxis(): Axis | undefined;
  setXAxis(value?: Axis): TimeseriesChart;
  hasXAxis(): boolean;
  clearXAxis(): TimeseriesChart;

  getYAxis(): Axis | undefined;
  setYAxis(value?: Axis): TimeseriesChart;
  hasYAxis(): boolean;
  clearYAxis(): TimeseriesChart;

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
    setValue(value: string): Timeseries;

    getSeries(): string;
    setSeries(value: string): Timeseries;

    getStackBySeries(): boolean;
    setStackBySeries(value: boolean): Timeseries;

    getMode(): TimeseriesChart.Timeseries.Mode;
    setMode(value: TimeseriesChart.Timeseries.Mode): Timeseries;

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
  setSpec(value: string): VegaChart;

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
  setDisplayColumnsList(value: Array<Table.ColumnDisplay>): Table;
  clearDisplayColumnsList(): Table;
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
    setName(value: string): ColumnDisplay;

    getDisplayTitle(): string;
    setDisplayTitle(value: string): ColumnDisplay;

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
  setDotColumn(value: string): Graph;

  getAdjacencyList(): Graph.AdjacencyList | undefined;
  setAdjacencyList(value?: Graph.AdjacencyList): Graph;
  hasAdjacencyList(): boolean;
  clearAdjacencyList(): Graph;

  getEdgeWeightColumn(): string;
  setEdgeWeightColumn(value: string): Graph;

  getNodeWeightColumn(): string;
  setNodeWeightColumn(value: string): Graph;

  getEdgeColorColumn(): string;
  setEdgeColorColumn(value: string): Graph;

  getEdgeThresholds(): Graph.EdgeThresholds | undefined;
  setEdgeThresholds(value?: Graph.EdgeThresholds): Graph;
  hasEdgeThresholds(): boolean;
  clearEdgeThresholds(): Graph;

  getEdgeHoverInfoList(): Array<string>;
  setEdgeHoverInfoList(value: Array<string>): Graph;
  clearEdgeHoverInfoList(): Graph;
  addEdgeHoverInfo(value: string, index?: number): Graph;

  getEdgeLength(): number;
  setEdgeLength(value: number): Graph;

  getEnableDefaultHierarchy(): boolean;
  setEnableDefaultHierarchy(value: boolean): Graph;

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
    edgeWeightColumn: string,
    nodeWeightColumn: string,
    edgeColorColumn: string,
    edgeThresholds?: Graph.EdgeThresholds.AsObject,
    edgeHoverInfoList: Array<string>,
    edgeLength: number,
    enableDefaultHierarchy: boolean,
  }

  export class AdjacencyList extends jspb.Message {
    getFromColumn(): string;
    setFromColumn(value: string): AdjacencyList;

    getToColumn(): string;
    setToColumn(value: string): AdjacencyList;

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


  export class EdgeThresholds extends jspb.Message {
    getMediumThreshold(): number;
    setMediumThreshold(value: number): EdgeThresholds;

    getHighThreshold(): number;
    setHighThreshold(value: number): EdgeThresholds;

    serializeBinary(): Uint8Array;
    toObject(includeInstance?: boolean): EdgeThresholds.AsObject;
    static toObject(includeInstance: boolean, msg: EdgeThresholds): EdgeThresholds.AsObject;
    static serializeBinaryToWriter(message: EdgeThresholds, writer: jspb.BinaryWriter): void;
    static deserializeBinary(bytes: Uint8Array): EdgeThresholds;
    static deserializeBinaryFromReader(message: EdgeThresholds, reader: jspb.BinaryReader): EdgeThresholds;
  }

  export namespace EdgeThresholds {
    export type AsObject = {
      mediumThreshold: number,
      highThreshold: number,
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
  setRequestorPodColumn(value: string): RequestGraph;

  getResponderPodColumn(): string;
  setResponderPodColumn(value: string): RequestGraph;

  getRequestorServiceColumn(): string;
  setRequestorServiceColumn(value: string): RequestGraph;

  getResponderServiceColumn(): string;
  setResponderServiceColumn(value: string): RequestGraph;

  getP50Column(): string;
  setP50Column(value: string): RequestGraph;

  getP90Column(): string;
  setP90Column(value: string): RequestGraph;

  getP99Column(): string;
  setP99Column(value: string): RequestGraph;

  getErrorRateColumn(): string;
  setErrorRateColumn(value: string): RequestGraph;

  getRequestsPerSecondColumn(): string;
  setRequestsPerSecondColumn(value: string): RequestGraph;

  getInboundBytesPerSecondColumn(): string;
  setInboundBytesPerSecondColumn(value: string): RequestGraph;

  getOutboundBytesPerSecondColumn(): string;
  setOutboundBytesPerSecondColumn(value: string): RequestGraph;

  getTotalRequestCountColumn(): string;
  setTotalRequestCountColumn(value: string): RequestGraph;

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

export class StackTraceFlameGraph extends jspb.Message {
  getStacktraceColumn(): string;
  setStacktraceColumn(value: string): StackTraceFlameGraph;

  getCountColumn(): string;
  setCountColumn(value: string): StackTraceFlameGraph;

  getPercentageColumn(): string;
  setPercentageColumn(value: string): StackTraceFlameGraph;

  getNamespaceColumn(): string;
  setNamespaceColumn(value: string): StackTraceFlameGraph;

  getPodColumn(): string;
  setPodColumn(value: string): StackTraceFlameGraph;

  getContainerColumn(): string;
  setContainerColumn(value: string): StackTraceFlameGraph;

  getPidColumn(): string;
  setPidColumn(value: string): StackTraceFlameGraph;

  getNodeColumn(): string;
  setNodeColumn(value: string): StackTraceFlameGraph;

  serializeBinary(): Uint8Array;
  toObject(includeInstance?: boolean): StackTraceFlameGraph.AsObject;
  static toObject(includeInstance: boolean, msg: StackTraceFlameGraph): StackTraceFlameGraph.AsObject;
  static serializeBinaryToWriter(message: StackTraceFlameGraph, writer: jspb.BinaryWriter): void;
  static deserializeBinary(bytes: Uint8Array): StackTraceFlameGraph;
  static deserializeBinaryFromReader(message: StackTraceFlameGraph, reader: jspb.BinaryReader): StackTraceFlameGraph;
}

export namespace StackTraceFlameGraph {
  export type AsObject = {
    stacktraceColumn: string,
    countColumn: string,
    percentageColumn: string,
    namespaceColumn: string,
    podColumn: string,
    containerColumn: string,
    pidColumn: string,
    nodeColumn: string,
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
  PX_LIST = 2000,
  PX_STRING_LIST = 2001,
}
