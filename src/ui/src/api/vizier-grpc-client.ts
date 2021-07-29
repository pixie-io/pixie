/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

import { Observable, of } from 'rxjs';
import {
  bufferTime, catchError, finalize, concatMap, map, timeout, startWith,
} from 'rxjs/operators';
import {
  ErrorDetails, ExecuteScriptRequest, HealthCheckRequest, QueryExecutionStats, Relation,
  RowBatchData, Status, MutationInfo, HealthCheckResponse, ExecuteScriptResponse,
} from 'app/types/generated/vizierapi_pb';
import { VizierServiceClient } from 'app/types/generated/VizierapiServiceClientPb';
import { VizierQueryError } from './vizier';

const noop = () => {};

declare global {
  interface Window {
    __GRPCWEB_DEVTOOLS__: (any) => void;
  }
}

function withDevTools(client) {
  // eslint-disable-next-line no-underscore-dangle
  const enableDevTools = globalThis.__GRPCWEB_DEVTOOLS__ || noop;
  enableDevTools([client]);
}

export interface Table {
  relation: Relation;
  data: RowBatchData[];
  name: string;
  id: string;
}

export interface VizierQueryResult {
  queryId?: string;
  tables: Table[];
  status?: Status;
  executionStats?: QueryExecutionStats;
  mutationInfo?: MutationInfo;
  schemaOnly?: boolean; // Whether the result only has the schema loaded so far.
}

export interface VizierQueryArg {
  name: string;
  value?: string;
  variable?: string;
}

export interface VizierQueryFunc {
  name: string;
  outputTablePrefix: string;
  args: VizierQueryArg[];
}

export interface BatchDataUpdate {
  id: string;
  name: string;
  relation: Relation;
  batch: RowBatchData;
}

interface ExecutionStartEvent {
  type: 'start';
}

interface ExecutionErrorEvent {
  type: 'error';
  error: VizierQueryError;
}

interface ExecutionMetadataEvent {
  type: 'metadata';
  table: Table;
}

interface ExecutionMutationInfoEvent {
  type: 'mutation-info';
  mutationInfo: MutationInfo;
}

interface ExecutionDataEvent {
  type: 'data';
  data: BatchDataUpdate[];
}

interface ExecutionCancelEvent {
  type: 'cancel';
}

interface ExecutionStatusEvent {
  type: 'status';
  status: Status;
}

interface ExecutionStatsEvent {
  type: 'stats';
  stats: QueryExecutionStats;
}

export type ExecutionEvent =
  ExecutionStartEvent
  | ExecutionErrorEvent
  | ExecutionMetadataEvent
  | ExecutionMutationInfoEvent
  | ExecutionDataEvent
  | ExecutionCancelEvent
  | ExecutionStatusEvent
  | ExecutionStatsEvent;

/** The latest state of an executeScript, streamed from an observable */
export interface ExecutionStateUpdate {
  /** The event that generated this state update */
  event: ExecutionEvent;
  /** If `completionReason` is not set, this result can be partial */
  results: VizierQueryResult;
  /** If set, cancels the execution. Gets unset when the query completes for any reason (including error or cancel).  */
  cancel?: () => void;
  /** If set, execution has halted for this reason */
  completionReason?: 'complete' | 'cancelled' | 'error';
}

function getExecutionErrors(errList: ErrorDetails[]): string[] {
  return errList.map((error) => {
    switch (error.getErrorCase()) {
      case ErrorDetails.ErrorCase.COMPILER_ERROR: {
        const ce = error.getCompilerError();
        return `Compiler error on line ${ce.getLine()}, column ${ce.getColumn()}: ${ce.getMessage()}.`;
      }
      default:
        return `Unknown error type ${ErrorDetails.ErrorCase[error.getErrorCase()]}.`;
    }
  });
}

function addFuncsToRequest(req: ExecuteScriptRequest, funcs: VizierQueryFunc[]): VizierQueryError {
  for (const input of funcs) {
    const execFuncPb = new ExecuteScriptRequest.FuncToExecute();
    execFuncPb.setFuncName(input.name);
    execFuncPb.setOutputTablePrefix(input.outputTablePrefix);
    for (const arg of input.args) {
      const argValPb = new ExecuteScriptRequest.FuncToExecute.ArgValue();
      argValPb.setName(arg.name);
      if (typeof arg.value === 'undefined') {
        return new VizierQueryError('vis', `No value provided for arg ${arg.name}.`);
      }
      if (typeof arg.value !== 'string') {
        return new VizierQueryError('vis', 'All args must be strings.'
          + ` Received '${typeof arg.value}' for arg '${arg.name}'.`);
      }
      argValPb.setValue(arg.value);
      execFuncPb.addArgValues(argValPb);
    }
    req.addExecFuncs(execFuncPb);
  }
  return null;
}

const HEALTH_CHECK_TIMEOUT = 10000; // 10 seconds

/**
 * Client for gRPC connections to individual Vizier clusters.
 * See CloudGQLClient for the GraphQL client that works with shared data.
 * Should not be used directly - PixieAPIClient exposes wrappers around both this and CloudGQLClient,
 * which smooth out bumps in things like error handling. Prefer using that if you can.
 */
export class VizierGRPCClient {
  private readonly client: VizierServiceClient;

  constructor(
    addr: string,
    private token: string,
    readonly clusterID: string,
    private attachCreds: boolean,
  ) {
    this.client = new VizierServiceClient(addr, null, attachCreds ? { withCredentials: 'true' } : {});
    withDevTools(this.client);
  }

  /**
    * Monitors the health of both Pixie instrumentation on a cluster, and this client's connection to it.
    * The returned Observable watches
    */
  health(): Observable<Status> {
    const headers = {
      ...(this.attachCreds ? {} : { Authorization: `bearer ${this.token}` }),
    };
    const req = new HealthCheckRequest();
    req.setClusterId(this.clusterID);
    const call = this.client.healthCheck(req, headers);
    return new Observable<HealthCheckResponse>((observer) => {
      call.on('data', observer.next.bind(observer));
      call.on('error', observer.error.bind(observer));
      call.on('end', observer.complete.bind(observer));
    }).pipe(
      map((resp: HealthCheckResponse) => resp.getStatus()),
      finalize((() => { call.cancel(); })),
      timeout(HEALTH_CHECK_TIMEOUT),
    );
  }

  // Use a generator to produce the VizierQueryFunc to remove the dependency on vis.tsx.
  // funcsGenerator should correspond to getQueryFuncs in vis.tsx.
  executeScript(
    script: string,
    funcs: VizierQueryFunc[],
    mutation: boolean,
  ): Observable<ExecutionStateUpdate> {
    const headers = {
      ...(this.attachCreds ? {} : { Authorization: `bearer ${this.token}` }),
    };
    const req = new ExecuteScriptRequest();
    req.setClusterId(this.clusterID);
    req.setQueryStr(script);
    req.setMutation(mutation);
    const err = addFuncsToRequest(req, funcs);
    if (err) {
      return of({
        event: { type: 'error', error: err }, completionReason: 'error',
      } as ExecutionStateUpdate);
    }

    const call = this.client.executeScript(req, headers);

    const tablesMap = new Map<string, Table>();
    const results: VizierQueryResult = { tables: [] };

    return new Observable<ExecuteScriptResponse>((observer) => {
      call.on('data', observer.next.bind(observer));
      call.on('error', observer.error.bind(observer));
      call.on('end', observer.complete.bind(observer));
      call.on('status', (status) => {
        if (status.code > 0) {
          observer.error(new VizierQueryError('server', status.details));
        }
      });
    }).pipe(
      finalize((() => { call.cancel(); })),
      bufferTime(250),
      concatMap((resps: ExecuteScriptResponse[]) => {
        const outs: ExecutionStateUpdate[] = [];
        const dataBatch: BatchDataUpdate[] = [];

        for (const resp of resps) {
          if (!results.queryId) {
            results.queryId = resp.getQueryId();
          }

          if (resp.hasStatus()) {
            const status = resp.getStatus();
            const errList = status.getErrorDetailsList();
            if (errList.length > 0) {
              throw new VizierQueryError('execution', getExecutionErrors(errList), status);
            }
            const errMsg = status.getMessage();
            if (errMsg) {
              throw new VizierQueryError('execution', errMsg, status);
            }

            results.status = status;
            outs.push({ event: { type: 'status', status }, results });
          }

          if (resp.hasMetaData()) {
            const relation = resp.getMetaData().getRelation();
            const id = resp.getMetaData().getId();
            const name = resp.getMetaData().getName();
            tablesMap.set(id, {
              relation, id, name, data: [],
            });
            const table = tablesMap.get(id);
            results.tables.push(table);
            results.schemaOnly = true;
            outs.push({ event: { type: 'metadata', table }, results });
          } else if (resp.hasMutationInfo()) {
            results.mutationInfo = resp.getMutationInfo();
            outs.push({ event: { type: 'mutation-info', mutationInfo: results.mutationInfo }, results });
          } else if (resp.hasData()) {
            const data = resp.getData();
            if (data.hasBatch()) {
              results.schemaOnly = false;

              const batch = data.getBatch();
              const id = batch.getTableId();
              const table = tablesMap.get(id);
              const { name, relation } = table;
              table.data.push(batch);
              dataBatch.push({
                id, name, relation, batch,
              });
            } else if (data.hasExecutionStats()) {
              // The query finished executing, and all the data has been received.
              results.executionStats = data.getExecutionStats();
              outs.push({
                event: { type: 'stats', stats: results.executionStats },
                completionReason: 'complete',
                results,
              });
            }
          }
        }

        outs.push({ event: { type: 'data', data: dataBatch }, results });
        return outs;
      }),
      startWith({
        event: { type: 'start' },
        results,
        cancel: () => call.cancel?.(),
      } as ExecutionStateUpdate),
      catchError((error) => {
        call.cancel();
        return of({
          event: { type: 'error', error }, completionReason: 'error',
        } as ExecutionStateUpdate);
      }),
      finalize((() => { call.cancel(); })),
    );
  }
}
