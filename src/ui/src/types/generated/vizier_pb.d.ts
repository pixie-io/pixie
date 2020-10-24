import * as jspb from "google-protobuf"


export class UInt128 extends jspb.Message {
  getLow(): number;
  setLow(value: number): void;

  getHigh(): number;
  setHigh(value: number): void;

  serializeBinary(): Uint8Array;
  toObject(includeInstance?: boolean): UInt128.AsObject;
  static toObject(includeInstance: boolean, msg: UInt128): UInt128.AsObject;
  static serializeBinaryToWriter(message: UInt128, writer: jspb.BinaryWriter): void;
  static deserializeBinary(bytes: Uint8Array): UInt128;
  static deserializeBinaryFromReader(message: UInt128, reader: jspb.BinaryReader): UInt128;
}

export namespace UInt128 {
  export type AsObject = {
    low: number,
    high: number,
  }
}

export class BooleanColumn extends jspb.Message {
  getDataList(): Array<boolean>;
  setDataList(value: Array<boolean>): void;
  clearDataList(): void;
  addData(value: boolean, index?: number): void;

  serializeBinary(): Uint8Array;
  toObject(includeInstance?: boolean): BooleanColumn.AsObject;
  static toObject(includeInstance: boolean, msg: BooleanColumn): BooleanColumn.AsObject;
  static serializeBinaryToWriter(message: BooleanColumn, writer: jspb.BinaryWriter): void;
  static deserializeBinary(bytes: Uint8Array): BooleanColumn;
  static deserializeBinaryFromReader(message: BooleanColumn, reader: jspb.BinaryReader): BooleanColumn;
}

export namespace BooleanColumn {
  export type AsObject = {
    dataList: Array<boolean>,
  }
}

export class Int64Column extends jspb.Message {
  getDataList(): Array<number>;
  setDataList(value: Array<number>): void;
  clearDataList(): void;
  addData(value: number, index?: number): void;

  serializeBinary(): Uint8Array;
  toObject(includeInstance?: boolean): Int64Column.AsObject;
  static toObject(includeInstance: boolean, msg: Int64Column): Int64Column.AsObject;
  static serializeBinaryToWriter(message: Int64Column, writer: jspb.BinaryWriter): void;
  static deserializeBinary(bytes: Uint8Array): Int64Column;
  static deserializeBinaryFromReader(message: Int64Column, reader: jspb.BinaryReader): Int64Column;
}

export namespace Int64Column {
  export type AsObject = {
    dataList: Array<number>,
  }
}

export class UInt128Column extends jspb.Message {
  getDataList(): Array<UInt128>;
  setDataList(value: Array<UInt128>): void;
  clearDataList(): void;
  addData(value?: UInt128, index?: number): UInt128;

  serializeBinary(): Uint8Array;
  toObject(includeInstance?: boolean): UInt128Column.AsObject;
  static toObject(includeInstance: boolean, msg: UInt128Column): UInt128Column.AsObject;
  static serializeBinaryToWriter(message: UInt128Column, writer: jspb.BinaryWriter): void;
  static deserializeBinary(bytes: Uint8Array): UInt128Column;
  static deserializeBinaryFromReader(message: UInt128Column, reader: jspb.BinaryReader): UInt128Column;
}

export namespace UInt128Column {
  export type AsObject = {
    dataList: Array<UInt128.AsObject>,
  }
}

export class Float64Column extends jspb.Message {
  getDataList(): Array<number>;
  setDataList(value: Array<number>): void;
  clearDataList(): void;
  addData(value: number, index?: number): void;

  serializeBinary(): Uint8Array;
  toObject(includeInstance?: boolean): Float64Column.AsObject;
  static toObject(includeInstance: boolean, msg: Float64Column): Float64Column.AsObject;
  static serializeBinaryToWriter(message: Float64Column, writer: jspb.BinaryWriter): void;
  static deserializeBinary(bytes: Uint8Array): Float64Column;
  static deserializeBinaryFromReader(message: Float64Column, reader: jspb.BinaryReader): Float64Column;
}

export namespace Float64Column {
  export type AsObject = {
    dataList: Array<number>,
  }
}

export class Time64NSColumn extends jspb.Message {
  getDataList(): Array<number>;
  setDataList(value: Array<number>): void;
  clearDataList(): void;
  addData(value: number, index?: number): void;

  serializeBinary(): Uint8Array;
  toObject(includeInstance?: boolean): Time64NSColumn.AsObject;
  static toObject(includeInstance: boolean, msg: Time64NSColumn): Time64NSColumn.AsObject;
  static serializeBinaryToWriter(message: Time64NSColumn, writer: jspb.BinaryWriter): void;
  static deserializeBinary(bytes: Uint8Array): Time64NSColumn;
  static deserializeBinaryFromReader(message: Time64NSColumn, reader: jspb.BinaryReader): Time64NSColumn;
}

export namespace Time64NSColumn {
  export type AsObject = {
    dataList: Array<number>,
  }
}

export class StringColumn extends jspb.Message {
  getDataList(): Array<string>;
  setDataList(value: Array<string>): void;
  clearDataList(): void;
  addData(value: string, index?: number): void;

  serializeBinary(): Uint8Array;
  toObject(includeInstance?: boolean): StringColumn.AsObject;
  static toObject(includeInstance: boolean, msg: StringColumn): StringColumn.AsObject;
  static serializeBinaryToWriter(message: StringColumn, writer: jspb.BinaryWriter): void;
  static deserializeBinary(bytes: Uint8Array): StringColumn;
  static deserializeBinaryFromReader(message: StringColumn, reader: jspb.BinaryReader): StringColumn;
}

export namespace StringColumn {
  export type AsObject = {
    dataList: Array<string>,
  }
}

export class Column extends jspb.Message {
  getBooleanData(): BooleanColumn | undefined;
  setBooleanData(value?: BooleanColumn): void;
  hasBooleanData(): boolean;
  clearBooleanData(): void;

  getInt64Data(): Int64Column | undefined;
  setInt64Data(value?: Int64Column): void;
  hasInt64Data(): boolean;
  clearInt64Data(): void;

  getUint128Data(): UInt128Column | undefined;
  setUint128Data(value?: UInt128Column): void;
  hasUint128Data(): boolean;
  clearUint128Data(): void;

  getTime64nsData(): Time64NSColumn | undefined;
  setTime64nsData(value?: Time64NSColumn): void;
  hasTime64nsData(): boolean;
  clearTime64nsData(): void;

  getFloat64Data(): Float64Column | undefined;
  setFloat64Data(value?: Float64Column): void;
  hasFloat64Data(): boolean;
  clearFloat64Data(): void;

  getStringData(): StringColumn | undefined;
  setStringData(value?: StringColumn): void;
  hasStringData(): boolean;
  clearStringData(): void;

  getColDataCase(): Column.ColDataCase;

  serializeBinary(): Uint8Array;
  toObject(includeInstance?: boolean): Column.AsObject;
  static toObject(includeInstance: boolean, msg: Column): Column.AsObject;
  static serializeBinaryToWriter(message: Column, writer: jspb.BinaryWriter): void;
  static deserializeBinary(bytes: Uint8Array): Column;
  static deserializeBinaryFromReader(message: Column, reader: jspb.BinaryReader): Column;
}

export namespace Column {
  export type AsObject = {
    booleanData?: BooleanColumn.AsObject,
    int64Data?: Int64Column.AsObject,
    uint128Data?: UInt128Column.AsObject,
    time64nsData?: Time64NSColumn.AsObject,
    float64Data?: Float64Column.AsObject,
    stringData?: StringColumn.AsObject,
  }

  export enum ColDataCase { 
    COL_DATA_NOT_SET = 0,
    BOOLEAN_DATA = 1,
    INT64_DATA = 2,
    UINT128_DATA = 3,
    TIME64NS_DATA = 4,
    FLOAT64_DATA = 5,
    STRING_DATA = 6,
  }
}

export class RowBatchData extends jspb.Message {
  getTableId(): string;
  setTableId(value: string): void;

  getColsList(): Array<Column>;
  setColsList(value: Array<Column>): void;
  clearColsList(): void;
  addCols(value?: Column, index?: number): Column;

  getNumRows(): number;
  setNumRows(value: number): void;

  getEow(): boolean;
  setEow(value: boolean): void;

  getEos(): boolean;
  setEos(value: boolean): void;

  serializeBinary(): Uint8Array;
  toObject(includeInstance?: boolean): RowBatchData.AsObject;
  static toObject(includeInstance: boolean, msg: RowBatchData): RowBatchData.AsObject;
  static serializeBinaryToWriter(message: RowBatchData, writer: jspb.BinaryWriter): void;
  static deserializeBinary(bytes: Uint8Array): RowBatchData;
  static deserializeBinaryFromReader(message: RowBatchData, reader: jspb.BinaryReader): RowBatchData;
}

export namespace RowBatchData {
  export type AsObject = {
    tableId: string,
    colsList: Array<Column.AsObject>,
    numRows: number,
    eow: boolean,
    eos: boolean,
  }
}

export class Relation extends jspb.Message {
  getColumnsList(): Array<Relation.ColumnInfo>;
  setColumnsList(value: Array<Relation.ColumnInfo>): void;
  clearColumnsList(): void;
  addColumns(value?: Relation.ColumnInfo, index?: number): Relation.ColumnInfo;

  serializeBinary(): Uint8Array;
  toObject(includeInstance?: boolean): Relation.AsObject;
  static toObject(includeInstance: boolean, msg: Relation): Relation.AsObject;
  static serializeBinaryToWriter(message: Relation, writer: jspb.BinaryWriter): void;
  static deserializeBinary(bytes: Uint8Array): Relation;
  static deserializeBinaryFromReader(message: Relation, reader: jspb.BinaryReader): Relation;
}

export namespace Relation {
  export type AsObject = {
    columnsList: Array<Relation.ColumnInfo.AsObject>,
  }

  export class ColumnInfo extends jspb.Message {
    getColumnName(): string;
    setColumnName(value: string): void;

    getColumnType(): DataType;
    setColumnType(value: DataType): void;

    getColumnDesc(): string;
    setColumnDesc(value: string): void;

    getColumnSemanticType(): SemanticType;
    setColumnSemanticType(value: SemanticType): void;

    serializeBinary(): Uint8Array;
    toObject(includeInstance?: boolean): ColumnInfo.AsObject;
    static toObject(includeInstance: boolean, msg: ColumnInfo): ColumnInfo.AsObject;
    static serializeBinaryToWriter(message: ColumnInfo, writer: jspb.BinaryWriter): void;
    static deserializeBinary(bytes: Uint8Array): ColumnInfo;
    static deserializeBinaryFromReader(message: ColumnInfo, reader: jspb.BinaryReader): ColumnInfo;
  }

  export namespace ColumnInfo {
    export type AsObject = {
      columnName: string,
      columnType: DataType,
      columnDesc: string,
      columnSemanticType: SemanticType,
    }
  }

}

export class CompilerError extends jspb.Message {
  getLine(): number;
  setLine(value: number): void;

  getColumn(): number;
  setColumn(value: number): void;

  getMessage(): string;
  setMessage(value: string): void;

  serializeBinary(): Uint8Array;
  toObject(includeInstance?: boolean): CompilerError.AsObject;
  static toObject(includeInstance: boolean, msg: CompilerError): CompilerError.AsObject;
  static serializeBinaryToWriter(message: CompilerError, writer: jspb.BinaryWriter): void;
  static deserializeBinary(bytes: Uint8Array): CompilerError;
  static deserializeBinaryFromReader(message: CompilerError, reader: jspb.BinaryReader): CompilerError;
}

export namespace CompilerError {
  export type AsObject = {
    line: number,
    column: number,
    message: string,
  }
}

export class ErrorDetails extends jspb.Message {
  getCompilerError(): CompilerError | undefined;
  setCompilerError(value?: CompilerError): void;
  hasCompilerError(): boolean;
  clearCompilerError(): void;

  getErrorCase(): ErrorDetails.ErrorCase;

  serializeBinary(): Uint8Array;
  toObject(includeInstance?: boolean): ErrorDetails.AsObject;
  static toObject(includeInstance: boolean, msg: ErrorDetails): ErrorDetails.AsObject;
  static serializeBinaryToWriter(message: ErrorDetails, writer: jspb.BinaryWriter): void;
  static deserializeBinary(bytes: Uint8Array): ErrorDetails;
  static deserializeBinaryFromReader(message: ErrorDetails, reader: jspb.BinaryReader): ErrorDetails;
}

export namespace ErrorDetails {
  export type AsObject = {
    compilerError?: CompilerError.AsObject,
  }

  export enum ErrorCase { 
    ERROR_NOT_SET = 0,
    COMPILER_ERROR = 1,
  }
}

export class Status extends jspb.Message {
  getCode(): number;
  setCode(value: number): void;

  getMessage(): string;
  setMessage(value: string): void;

  getErrorDetailsList(): Array<ErrorDetails>;
  setErrorDetailsList(value: Array<ErrorDetails>): void;
  clearErrorDetailsList(): void;
  addErrorDetails(value?: ErrorDetails, index?: number): ErrorDetails;

  serializeBinary(): Uint8Array;
  toObject(includeInstance?: boolean): Status.AsObject;
  static toObject(includeInstance: boolean, msg: Status): Status.AsObject;
  static serializeBinaryToWriter(message: Status, writer: jspb.BinaryWriter): void;
  static deserializeBinary(bytes: Uint8Array): Status;
  static deserializeBinaryFromReader(message: Status, reader: jspb.BinaryReader): Status;
}

export namespace Status {
  export type AsObject = {
    code: number,
    message: string,
    errorDetailsList: Array<ErrorDetails.AsObject>,
  }
}

export class ScalarValue extends jspb.Message {
  getDataType(): DataType;
  setDataType(value: DataType): void;

  getBoolValue(): boolean;
  setBoolValue(value: boolean): void;

  getInt64Value(): number;
  setInt64Value(value: number): void;

  getFloat64Value(): number;
  setFloat64Value(value: number): void;

  getStringValue(): string;
  setStringValue(value: string): void;

  getTime64NsValue(): number;
  setTime64NsValue(value: number): void;

  getUint128Value(): UInt128 | undefined;
  setUint128Value(value?: UInt128): void;
  hasUint128Value(): boolean;
  clearUint128Value(): void;

  getValueCase(): ScalarValue.ValueCase;

  serializeBinary(): Uint8Array;
  toObject(includeInstance?: boolean): ScalarValue.AsObject;
  static toObject(includeInstance: boolean, msg: ScalarValue): ScalarValue.AsObject;
  static serializeBinaryToWriter(message: ScalarValue, writer: jspb.BinaryWriter): void;
  static deserializeBinary(bytes: Uint8Array): ScalarValue;
  static deserializeBinaryFromReader(message: ScalarValue, reader: jspb.BinaryReader): ScalarValue;
}

export namespace ScalarValue {
  export type AsObject = {
    dataType: DataType,
    boolValue: boolean,
    int64Value: number,
    float64Value: number,
    stringValue: string,
    time64NsValue: number,
    uint128Value?: UInt128.AsObject,
  }

  export enum ValueCase { 
    VALUE_NOT_SET = 0,
    BOOL_VALUE = 2,
    INT64_VALUE = 3,
    FLOAT64_VALUE = 4,
    STRING_VALUE = 5,
    TIME64_NS_VALUE = 6,
    UINT128_VALUE = 7,
  }
}

export class ExecuteScriptRequest extends jspb.Message {
  getQueryStr(): string;
  setQueryStr(value: string): void;

  getClusterId(): string;
  setClusterId(value: string): void;

  getExecFuncsList(): Array<ExecuteScriptRequest.FuncToExecute>;
  setExecFuncsList(value: Array<ExecuteScriptRequest.FuncToExecute>): void;
  clearExecFuncsList(): void;
  addExecFuncs(value?: ExecuteScriptRequest.FuncToExecute, index?: number): ExecuteScriptRequest.FuncToExecute;

  getMutation(): boolean;
  setMutation(value: boolean): void;

  serializeBinary(): Uint8Array;
  toObject(includeInstance?: boolean): ExecuteScriptRequest.AsObject;
  static toObject(includeInstance: boolean, msg: ExecuteScriptRequest): ExecuteScriptRequest.AsObject;
  static serializeBinaryToWriter(message: ExecuteScriptRequest, writer: jspb.BinaryWriter): void;
  static deserializeBinary(bytes: Uint8Array): ExecuteScriptRequest;
  static deserializeBinaryFromReader(message: ExecuteScriptRequest, reader: jspb.BinaryReader): ExecuteScriptRequest;
}

export namespace ExecuteScriptRequest {
  export type AsObject = {
    queryStr: string,
    clusterId: string,
    execFuncsList: Array<ExecuteScriptRequest.FuncToExecute.AsObject>,
    mutation: boolean,
  }

  export class FuncToExecute extends jspb.Message {
    getFuncName(): string;
    setFuncName(value: string): void;

    getArgValuesList(): Array<ExecuteScriptRequest.FuncToExecute.ArgValue>;
    setArgValuesList(value: Array<ExecuteScriptRequest.FuncToExecute.ArgValue>): void;
    clearArgValuesList(): void;
    addArgValues(value?: ExecuteScriptRequest.FuncToExecute.ArgValue, index?: number): ExecuteScriptRequest.FuncToExecute.ArgValue;

    getOutputTablePrefix(): string;
    setOutputTablePrefix(value: string): void;

    serializeBinary(): Uint8Array;
    toObject(includeInstance?: boolean): FuncToExecute.AsObject;
    static toObject(includeInstance: boolean, msg: FuncToExecute): FuncToExecute.AsObject;
    static serializeBinaryToWriter(message: FuncToExecute, writer: jspb.BinaryWriter): void;
    static deserializeBinary(bytes: Uint8Array): FuncToExecute;
    static deserializeBinaryFromReader(message: FuncToExecute, reader: jspb.BinaryReader): FuncToExecute;
  }

  export namespace FuncToExecute {
    export type AsObject = {
      funcName: string,
      argValuesList: Array<ExecuteScriptRequest.FuncToExecute.ArgValue.AsObject>,
      outputTablePrefix: string,
    }

    export class ArgValue extends jspb.Message {
      getName(): string;
      setName(value: string): void;

      getValue(): string;
      setValue(value: string): void;

      serializeBinary(): Uint8Array;
      toObject(includeInstance?: boolean): ArgValue.AsObject;
      static toObject(includeInstance: boolean, msg: ArgValue): ArgValue.AsObject;
      static serializeBinaryToWriter(message: ArgValue, writer: jspb.BinaryWriter): void;
      static deserializeBinary(bytes: Uint8Array): ArgValue;
      static deserializeBinaryFromReader(message: ArgValue, reader: jspb.BinaryReader): ArgValue;
    }

    export namespace ArgValue {
      export type AsObject = {
        name: string,
        value: string,
      }
    }

  }

}

export class QueryTimingInfo extends jspb.Message {
  getExecutionTimeNs(): number;
  setExecutionTimeNs(value: number): void;

  getCompilationTimeNs(): number;
  setCompilationTimeNs(value: number): void;

  serializeBinary(): Uint8Array;
  toObject(includeInstance?: boolean): QueryTimingInfo.AsObject;
  static toObject(includeInstance: boolean, msg: QueryTimingInfo): QueryTimingInfo.AsObject;
  static serializeBinaryToWriter(message: QueryTimingInfo, writer: jspb.BinaryWriter): void;
  static deserializeBinary(bytes: Uint8Array): QueryTimingInfo;
  static deserializeBinaryFromReader(message: QueryTimingInfo, reader: jspb.BinaryReader): QueryTimingInfo;
}

export namespace QueryTimingInfo {
  export type AsObject = {
    executionTimeNs: number,
    compilationTimeNs: number,
  }
}

export class QueryExecutionStats extends jspb.Message {
  getTiming(): QueryTimingInfo | undefined;
  setTiming(value?: QueryTimingInfo): void;
  hasTiming(): boolean;
  clearTiming(): void;

  getBytesProcessed(): number;
  setBytesProcessed(value: number): void;

  getRecordsProcessed(): number;
  setRecordsProcessed(value: number): void;

  serializeBinary(): Uint8Array;
  toObject(includeInstance?: boolean): QueryExecutionStats.AsObject;
  static toObject(includeInstance: boolean, msg: QueryExecutionStats): QueryExecutionStats.AsObject;
  static serializeBinaryToWriter(message: QueryExecutionStats, writer: jspb.BinaryWriter): void;
  static deserializeBinary(bytes: Uint8Array): QueryExecutionStats;
  static deserializeBinaryFromReader(message: QueryExecutionStats, reader: jspb.BinaryReader): QueryExecutionStats;
}

export namespace QueryExecutionStats {
  export type AsObject = {
    timing?: QueryTimingInfo.AsObject,
    bytesProcessed: number,
    recordsProcessed: number,
  }
}

export class QueryMetadata extends jspb.Message {
  getRelation(): Relation | undefined;
  setRelation(value?: Relation): void;
  hasRelation(): boolean;
  clearRelation(): void;

  getName(): string;
  setName(value: string): void;

  getId(): string;
  setId(value: string): void;

  serializeBinary(): Uint8Array;
  toObject(includeInstance?: boolean): QueryMetadata.AsObject;
  static toObject(includeInstance: boolean, msg: QueryMetadata): QueryMetadata.AsObject;
  static serializeBinaryToWriter(message: QueryMetadata, writer: jspb.BinaryWriter): void;
  static deserializeBinary(bytes: Uint8Array): QueryMetadata;
  static deserializeBinaryFromReader(message: QueryMetadata, reader: jspb.BinaryReader): QueryMetadata;
}

export namespace QueryMetadata {
  export type AsObject = {
    relation?: Relation.AsObject,
    name: string,
    id: string,
  }
}

export class QueryData extends jspb.Message {
  getBatch(): RowBatchData | undefined;
  setBatch(value?: RowBatchData): void;
  hasBatch(): boolean;
  clearBatch(): void;

  getExecutionStats(): QueryExecutionStats | undefined;
  setExecutionStats(value?: QueryExecutionStats): void;
  hasExecutionStats(): boolean;
  clearExecutionStats(): void;

  serializeBinary(): Uint8Array;
  toObject(includeInstance?: boolean): QueryData.AsObject;
  static toObject(includeInstance: boolean, msg: QueryData): QueryData.AsObject;
  static serializeBinaryToWriter(message: QueryData, writer: jspb.BinaryWriter): void;
  static deserializeBinary(bytes: Uint8Array): QueryData;
  static deserializeBinaryFromReader(message: QueryData, reader: jspb.BinaryReader): QueryData;
}

export namespace QueryData {
  export type AsObject = {
    batch?: RowBatchData.AsObject,
    executionStats?: QueryExecutionStats.AsObject,
  }
}

export class ExecuteScriptResponse extends jspb.Message {
  getStatus(): Status | undefined;
  setStatus(value?: Status): void;
  hasStatus(): boolean;
  clearStatus(): void;

  getQueryId(): string;
  setQueryId(value: string): void;

  getData(): QueryData | undefined;
  setData(value?: QueryData): void;
  hasData(): boolean;
  clearData(): void;

  getMetaData(): QueryMetadata | undefined;
  setMetaData(value?: QueryMetadata): void;
  hasMetaData(): boolean;
  clearMetaData(): void;

  getMutationInfo(): MutationInfo | undefined;
  setMutationInfo(value?: MutationInfo): void;
  hasMutationInfo(): boolean;
  clearMutationInfo(): void;

  getResultCase(): ExecuteScriptResponse.ResultCase;

  serializeBinary(): Uint8Array;
  toObject(includeInstance?: boolean): ExecuteScriptResponse.AsObject;
  static toObject(includeInstance: boolean, msg: ExecuteScriptResponse): ExecuteScriptResponse.AsObject;
  static serializeBinaryToWriter(message: ExecuteScriptResponse, writer: jspb.BinaryWriter): void;
  static deserializeBinary(bytes: Uint8Array): ExecuteScriptResponse;
  static deserializeBinaryFromReader(message: ExecuteScriptResponse, reader: jspb.BinaryReader): ExecuteScriptResponse;
}

export namespace ExecuteScriptResponse {
  export type AsObject = {
    status?: Status.AsObject,
    queryId: string,
    data?: QueryData.AsObject,
    metaData?: QueryMetadata.AsObject,
    mutationInfo?: MutationInfo.AsObject,
  }

  export enum ResultCase { 
    RESULT_NOT_SET = 0,
    DATA = 3,
    META_DATA = 4,
  }
}

export class MutationInfo extends jspb.Message {
  getStatus(): Status | undefined;
  setStatus(value?: Status): void;
  hasStatus(): boolean;
  clearStatus(): void;

  getStatesList(): Array<MutationInfo.MutationState>;
  setStatesList(value: Array<MutationInfo.MutationState>): void;
  clearStatesList(): void;
  addStates(value?: MutationInfo.MutationState, index?: number): MutationInfo.MutationState;

  serializeBinary(): Uint8Array;
  toObject(includeInstance?: boolean): MutationInfo.AsObject;
  static toObject(includeInstance: boolean, msg: MutationInfo): MutationInfo.AsObject;
  static serializeBinaryToWriter(message: MutationInfo, writer: jspb.BinaryWriter): void;
  static deserializeBinary(bytes: Uint8Array): MutationInfo;
  static deserializeBinaryFromReader(message: MutationInfo, reader: jspb.BinaryReader): MutationInfo;
}

export namespace MutationInfo {
  export type AsObject = {
    status?: Status.AsObject,
    statesList: Array<MutationInfo.MutationState.AsObject>,
  }

  export class MutationState extends jspb.Message {
    getId(): string;
    setId(value: string): void;

    getState(): LifeCycleState;
    setState(value: LifeCycleState): void;

    getName(): string;
    setName(value: string): void;

    serializeBinary(): Uint8Array;
    toObject(includeInstance?: boolean): MutationState.AsObject;
    static toObject(includeInstance: boolean, msg: MutationState): MutationState.AsObject;
    static serializeBinaryToWriter(message: MutationState, writer: jspb.BinaryWriter): void;
    static deserializeBinary(bytes: Uint8Array): MutationState;
    static deserializeBinaryFromReader(message: MutationState, reader: jspb.BinaryReader): MutationState;
  }

  export namespace MutationState {
    export type AsObject = {
      id: string,
      state: LifeCycleState,
      name: string,
    }
  }

}

export class HealthCheckRequest extends jspb.Message {
  getClusterId(): string;
  setClusterId(value: string): void;

  serializeBinary(): Uint8Array;
  toObject(includeInstance?: boolean): HealthCheckRequest.AsObject;
  static toObject(includeInstance: boolean, msg: HealthCheckRequest): HealthCheckRequest.AsObject;
  static serializeBinaryToWriter(message: HealthCheckRequest, writer: jspb.BinaryWriter): void;
  static deserializeBinary(bytes: Uint8Array): HealthCheckRequest;
  static deserializeBinaryFromReader(message: HealthCheckRequest, reader: jspb.BinaryReader): HealthCheckRequest;
}

export namespace HealthCheckRequest {
  export type AsObject = {
    clusterId: string,
  }
}

export class HealthCheckResponse extends jspb.Message {
  getStatus(): Status | undefined;
  setStatus(value?: Status): void;
  hasStatus(): boolean;
  clearStatus(): void;

  serializeBinary(): Uint8Array;
  toObject(includeInstance?: boolean): HealthCheckResponse.AsObject;
  static toObject(includeInstance: boolean, msg: HealthCheckResponse): HealthCheckResponse.AsObject;
  static serializeBinaryToWriter(message: HealthCheckResponse, writer: jspb.BinaryWriter): void;
  static deserializeBinary(bytes: Uint8Array): HealthCheckResponse;
  static deserializeBinaryFromReader(message: HealthCheckResponse, reader: jspb.BinaryReader): HealthCheckResponse;
}

export namespace HealthCheckResponse {
  export type AsObject = {
    status?: Status.AsObject,
  }
}

export class DebugLogRequest extends jspb.Message {
  getClusterId(): string;
  setClusterId(value: string): void;

  getPodName(): string;
  setPodName(value: string): void;

  getPrevious(): boolean;
  setPrevious(value: boolean): void;

  serializeBinary(): Uint8Array;
  toObject(includeInstance?: boolean): DebugLogRequest.AsObject;
  static toObject(includeInstance: boolean, msg: DebugLogRequest): DebugLogRequest.AsObject;
  static serializeBinaryToWriter(message: DebugLogRequest, writer: jspb.BinaryWriter): void;
  static deserializeBinary(bytes: Uint8Array): DebugLogRequest;
  static deserializeBinaryFromReader(message: DebugLogRequest, reader: jspb.BinaryReader): DebugLogRequest;
}

export namespace DebugLogRequest {
  export type AsObject = {
    clusterId: string,
    podName: string,
    previous: boolean,
  }
}

export class DebugLogResponse extends jspb.Message {
  getData(): string;
  setData(value: string): void;

  serializeBinary(): Uint8Array;
  toObject(includeInstance?: boolean): DebugLogResponse.AsObject;
  static toObject(includeInstance: boolean, msg: DebugLogResponse): DebugLogResponse.AsObject;
  static serializeBinaryToWriter(message: DebugLogResponse, writer: jspb.BinaryWriter): void;
  static deserializeBinary(bytes: Uint8Array): DebugLogResponse;
  static deserializeBinaryFromReader(message: DebugLogResponse, reader: jspb.BinaryReader): DebugLogResponse;
}

export namespace DebugLogResponse {
  export type AsObject = {
    data: string,
  }
}

export enum DataType { 
  DATA_TYPE_UNKNOWN = 0,
  BOOLEAN = 1,
  INT64 = 2,
  UINT128 = 3,
  FLOAT64 = 4,
  STRING = 5,
  TIME64NS = 6,
}
export enum SemanticType { 
  ST_UNSPECIFIED = 0,
  ST_NONE = 1,
  ST_AGENT_UID = 100,
  ST_UPID = 200,
  ST_SERVICE_NAME = 300,
  ST_POD_NAME = 400,
  ST_POD_PHASE = 401,
  ST_POD_STATUS = 402,
  ST_NODE_NAME = 500,
  ST_CONTAINER_NAME = 600,
  ST_CONTAINER_STATE = 601,
  ST_CONTAINER_STATUS = 602,
  ST_NAMESPACE_NAME = 700,
  ST_BYTES = 800,
  ST_PERCENT = 900,
  ST_DURATION_NS = 901,
  ST_THROUGHPUT_PER_NS = 902,
  ST_THROUGHPUT_BYTES_PER_NS = 903,
  ST_QUANTILES = 1000,
  ST_DURATION_NS_QUANTILES = 1001,
  ST_IP_ADDRESS = 1100,
  ST_PORT = 1200,
  ST_HTTP_REQ_METHOD = 1300,
  ST_HTTP_RESP_STATUS = 1400,
  ST_HTTP_RESP_MESSAGE = 1500,
}
export enum LifeCycleState { 
  UNKNOWN_STATE = 0,
  PENDING_STATE = 1,
  RUNNING_STATE = 2,
  FAILED_STATE = 3,
  TERMINATED_STATE = 4,
}
