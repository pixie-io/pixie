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
  }

  export class Variable extends jspb.Message {
    getName(): string;
    setName(value: string): void;

    getType(): PXType;
    setType(value: PXType): void;

    getDefaultValue(): string;
    setDefaultValue(value: string): void;

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

  getDisplaySpec(): google_protobuf_any_pb.Any | undefined;
  setDisplaySpec(value?: google_protobuf_any_pb.Any): void;
  hasDisplaySpec(): boolean;
  clearDisplaySpec(): void;

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
      MODE_BAR = 1,
      MODE_LINE = 2,
      MODE_POINT = 3,
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
  serializeBinary(): Uint8Array;
  toObject(includeInstance?: boolean): Table.AsObject;
  static toObject(includeInstance: boolean, msg: Table): Table.AsObject;
  static serializeBinaryToWriter(message: Table, writer: jspb.BinaryWriter): void;
  static deserializeBinary(bytes: Uint8Array): Table;
  static deserializeBinaryFromReader(message: Table, reader: jspb.BinaryReader): Table;
}

export namespace Table {
  export type AsObject = {
  }
}

export class Graph extends jspb.Message {
  getDotColumn(): string;
  setDotColumn(value: string): void;

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
  }
}

export enum PXType { 
  PX_UNKNOWN = 0,
  PX_BOOLEAN = 1,
  PX_INT64 = 2,
  PX_FLOAT64 = 3,
  PX_STRING = 4,
  PX_SERVICE = 1000,
}
