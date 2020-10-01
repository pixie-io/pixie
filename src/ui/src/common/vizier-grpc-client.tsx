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

const EXECUTE_SCRIPT_TIMEOUT = 5000; // 5 seconds
const HEALTHCHECK_TIMEOUT = 10000; // 10 seconds

export class VizierGRPCClient {
  private client: VizierServiceClient;

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
  executeScript(script: string, funcs: VizierQueryFunc[], mutation: boolean, onData, onError) {
    const headers = {
      ...(this.attachCreds ? {} : { Authorization: `BEARER ${this.token}` }),
    };

    let req: ExecuteScriptRequest;
    try {
      req = this.buildRequest(script, funcs, mutation);
    } catch (err) {
      onError(err);
      return () => {};
    }

    const call = this.client.executeScript(req, headers);
    const tablesMap = new Map<string, Table>();
    const results: VizierQueryResult = { tables: [] };
    let resolved = false;

    let lastFlush = new Date();

    call.on('data', (resp) => {
      if (!results.queryId) {
        results.queryId = resp.getQueryId();
      }

      if (resp.hasStatus()) {
        const status = resp.getStatus();
        const errList = status.getErrorDetailsList();
        if (errList.length > 0) {
          onError(new VizierQueryError('execution', getExecutionErrors(errList), status));
          return;
        }
        const errMsg = status.getMessage();
        if (errMsg) {
          onError(new VizierQueryError('execution', errMsg, status));
          return;
        }

        results.status = status;
        onData(results);
        resolved = true;
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
        onData(results);
      } else if (resp.hasMutationInfo()) {
        results.mutationInfo = resp.getMutationInfo();
      } else if (resp.hasData()) {
        const data = resp.getData();
        if (data.hasBatch()) {
          results.schemaOnly = false;

          const batch = data.getBatch();
          const id = batch.getTableId();
          const table = tablesMap.get(id);
          table.data.push(batch);

          // Only flush every 1s, to avoid excessive rerenders in the UI.
          if (new Date().getTime() - lastFlush.getTime() > 1000) {
            // TODO: This resends all batches to onData. In the future, we will want this to only send
            // the latest batch. This will require more extensive changes that require us to track the
            // full set of batches elsewhere. For now, updating to a subscription/callback model gets a
            // step closer to that point.
            onData(results);
            lastFlush = new Date();
          }
        } else if (data.hasExecutionStats()) {
          // The query finished executing, and all the data has been received.
          results.executionStats = data.getExecutionStats();
          onData(results);
          resolved = true;
        }
      }
    });

    call.on('end', () => {
      if (!resolved) {
        onError(new VizierQueryError('execution', 'Execution ended with incomplete results'));
      }
    });

    call.on('error', (err) => {
      onError(new VizierQueryError('server', err.message));
    });

    return () => (() => {
      call.cancel();
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
