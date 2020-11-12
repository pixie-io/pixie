import { VizierQueryError } from 'common/errors';
import { Observable, throwError } from 'rxjs';
import { catchError, timeout } from 'rxjs/operators';
import {
  ErrorDetails, ExecuteScriptRequest, HealthCheckRequest, QueryExecutionStats, Relation,
  RowBatchData, Status, MutationInfo,
} from 'types/generated/vizier_pb';
import { VizierServiceClient } from 'types/generated/VizierServiceClientPb';
import noop from 'utils/noop';

declare global {
  interface Window {
    __GRPCWEB_DEVTOOLS__: (any) => void;
  }
}

function withDevTools(client) {
  // eslint-disable-next-line no-underscore-dangle
  const enableDevTools = window.__GRPCWEB_DEVTOOLS__ || noop;
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
  /** Cancels the execution, if it is still running */
  cancel: () => void;
  /** If set, execution has halted for this reason */
  completionReason?: 'complete'|'cancelled'|'error';
  /** If set, execution has been halted by this error */
  error?: VizierQueryError;
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

const HEALTHCHECK_TIMEOUT = 10000; // 10 seconds

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

  health(): Observable<Status> {
    const headers = {
      ...(this.attachCreds ? {} : { Authorization: `BEARER ${this.token}` }),
    };
    const req = new HealthCheckRequest();
    req.setClusterId(this.clusterID);
    const call = this.client.healthCheck(req, headers);
    return new Observable<Status>((observer) => {
      call.on('data', (resp) => {
        if (observer.closed) {
          call.cancel();
        }
        observer.next(resp.getStatus());
      });

      call.on('error', (error) => {
        observer.error(error);
      });

      call.on('end', () => {
        observer.complete();
      });
    }).pipe(
      timeout(HEALTHCHECK_TIMEOUT),
      catchError((err) => {
        call.cancel();
        return throwError(err);
      }),
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
      ...(this.attachCreds ? {} : { Authorization: `BEARER ${this.token}` }),
    };

    return new Observable<ExecutionStateUpdate>((subscriber) => {
      let req: ExecuteScriptRequest;
      const results: VizierQueryResult = { tables: [] };
      try {
        req = this.buildRequest(script, funcs, mutation);
      } catch (error) {
        subscriber.next({
          event: { type: 'error', error }, results, cancel: () => { }, error, completionReason: 'error',
        });
        subscriber.complete();
        subscriber.unsubscribe();
        return;
      }

      const call = this.client.executeScript(req, headers);
      const tablesMap = new Map<string, Table>();
      let resolved = false;

      let awaitingUpdates: BatchDataUpdate[] = [];
      let updateInterval: number;

      const cancel = () => {
        clearInterval(updateInterval);
        call.cancel();
        subscriber.next({
          event: { type: 'cancel' },
          results,
          cancel,
          completionReason: 'cancelled',
        });
        subscriber.complete();
        subscriber.unsubscribe();
      };

      const emit = (
        event: ExecutionEvent,
        completionReason?: ExecutionStateUpdate['completionReason'],
        error?: VizierQueryError,
      ) => {
        subscriber.next({
          event, results, cancel, completionReason, error,
        });
      };

      const emitError = (error: VizierQueryError) => {
        clearInterval(updateInterval);
        call.cancel();
        emit({ type: 'error', error }, 'error', error);
        subscriber.complete();
        subscriber.unsubscribe();
      };

      updateInterval = window.setInterval(() => {
        if (awaitingUpdates.length) {
          emit({ type: 'data', data: awaitingUpdates });
          awaitingUpdates = [];
        }
      }, 1000);

      // To provide a cancel method immediately
      emit({ type: 'start' });

      call.on('data', (resp) => {
        if (!results.queryId) {
          results.queryId = resp.getQueryId();
        }

        if (resp.hasStatus()) {
          const status = resp.getStatus();
          const errList = status.getErrorDetailsList();
          resolved = true;
          if (errList.length > 0) {
            emitError(new VizierQueryError('execution', getExecutionErrors(errList), status));
            return;
          }
          const errMsg = status.getMessage();
          if (errMsg) {
            emitError(new VizierQueryError('execution', errMsg, status));
            return;
          }

          results.status = status;
          emit({ type: 'status', status });
          return;
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
          emit({ type: 'metadata', table });
        } else if (resp.hasMutationInfo()) {
          results.mutationInfo = resp.getMutationInfo();
          emit({ type: 'mutation-info', mutationInfo: results.mutationInfo });
        } else if (resp.hasData()) {
          const data = resp.getData();
          if (data.hasBatch()) {
            results.schemaOnly = false;

            const batch = data.getBatch();
            const id = batch.getTableId();
            const table = tablesMap.get(id);
            const { name, relation } = table;
            table.data.push(batch);

            // These get flushed on an interval, so as not to flood the UI with expensive re-renders
            awaitingUpdates.push({
              id, name, relation, batch,
            });
          } else if (data.hasExecutionStats()) {
            // The query finished executing, and all the data has been received.
            results.executionStats = data.getExecutionStats();
            emit({ type: 'stats', stats: results.executionStats }, 'complete');
            subscriber.complete();
            subscriber.unsubscribe();
            resolved = true;
          }
        }
      });

      call.on('end', () => {
        clearInterval(updateInterval);
        if (!resolved) {
          emitError(new VizierQueryError('execution', 'Execution ended with incomplete results'));
        }
      });

      call.on('error', (err) => {
        resolved = true;
        clearInterval(updateInterval);
        emitError(new VizierQueryError('server', err.message));
      });
      call.on('status', (status) => {
        if (status.code > 0) {
          resolved = true;
          emitError(new VizierQueryError('server', status.details));
        }
      });
    });
  }

  private buildRequest(script: string, funcs: VizierQueryFunc[], mutation: boolean): ExecuteScriptRequest {
    const req = new ExecuteScriptRequest();
    const errors = [];

    req.setClusterId(this.clusterID);
    req.setQueryStr(script);
    req.setMutation(mutation);
    funcs.forEach((input: VizierQueryFunc) => {
      const execFuncPb = new ExecuteScriptRequest.FuncToExecute();
      execFuncPb.setFuncName(input.name);
      execFuncPb.setOutputTablePrefix(input.outputTablePrefix);
      input.args.forEach((arg) => {
        const argValPb = new ExecuteScriptRequest.FuncToExecute.ArgValue();
        argValPb.setName(arg.name);
        if (typeof arg.value !== 'string') {
          errors.push(new VizierQueryError('vis', `No value provided for arg ${arg.name}.`));
          return;
        }
        argValPb.setValue(arg.value);
        execFuncPb.addArgValues(argValPb);
      });
      req.addExecFuncs(execFuncPb);
    });

    if (errors.length > 0) {
      // eslint-disable-next-line @typescript-eslint/no-throw-literal
      throw errors;
    }
    return req;
  }
}
