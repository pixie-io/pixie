import {VizierQueryError} from 'common/errors';
import {Observable} from 'rxjs';
import {
    ErrorDetails, ExecuteScriptRequest, HealthCheckRequest, QueryExecutionStats, Relation,
    RowBatchData, Status,
} from 'types/generated/vizier_pb';
import {VizierServiceClient} from 'types/generated/VizierServiceClientPb';

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

export class VizierGRPCClient {
  private client: VizierServiceClient;

  constructor(addr: string, private token: string, private clusterID: string, private attachCreds: boolean) {
    this.client = new VizierServiceClient(addr, null, attachCreds ? { withCredentials: 'true' } : {});
  }

  health(): Observable<Status> {
    return Observable.create((observer) => {
      const req = new HealthCheckRequest();
      req.setClusterId(this.clusterID);
      const call = this.client.healthCheck(req, this.attachCreds ? {} : { Authorization: `BEARER ${this.token}` });
      call.on('data', (resp) => {
        observer.next(resp.getStatus());
      });
      call.on('error', (error) => {
        observer.error(error);
      });
      call.on('end', () => {
        observer.complete();
      });
    });
  }

  executeScriptOld(script: string): Promise<VizierQueryResult> {
    return this.executeScript(script, []);
  }

  // Use a generator to produce the VizierQueryFunc to remove the dependency on vis.tsx.
  // funcsGenerator should correspond to getQueryFuncs in vis.tsx.
  executeScript(script: string, funcs: VizierQueryFunc[]): Promise<VizierQueryResult> {
    return new Promise((resolve, reject) => {
      let req: ExecuteScriptRequest;
      try {
        req = this.buildRequest(script, funcs);
      } catch (err) {
        reject(err);
        return;
      }

      const call = this.client.executeScript(req, this.attachCreds ? {} : { Authorization: `BEARER ${this.token}` });
      const tablesMap = new Map<string, Table>();
      const results: VizierQueryResult = { tables: [] };

      call.on('data', (resp) => {
        if (!results.queryId) {
          results.queryId = resp.getQueryId();
        }

        if (resp.hasStatus()) {
          const status = resp.getStatus();
          const errList = status.getErrorDetailsList();
          if (errList.length > 0) {
            reject(new VizierQueryError('execution', getExecutionErrors(errList)));
            return;
          }

          results.status = status;
          resolve(results);
          return;
        }

        if (resp.hasMetaData()) {
          const relation = resp.getMetaData().getRelation();
          const id = resp.getMetaData().getId();
          const name = resp.getMetaData().getName();
          tablesMap.set(id, { relation, id, name, data: [] });
        } else if (resp.hasData()) {
          const data = resp.getData();
          if (data.hasBatch()) {
            const batch = data.getBatch();
            const id = batch.getTableId();
            const table = tablesMap.get(id);
            if (!table) {
              throw new Error('table does not exisit');
            }
            // Append the data.
            table.data.push(batch);

            // The table is complete.
            if (batch.getEos()) {
              results.tables.push(table);
              tablesMap.delete(id);
              return;
            }
          } else if (data.hasExecutionStats()) {
            // The query finished executing, and all the data has been received.
            results.executionStats = data.getExecutionStats();
            resolve(results);
            return;
          }
        }
      });

      call.on('error', (err) => {
        reject(new VizierQueryError('server', err.message));
        return;
      });
    });
  }

  private buildRequest(script: string, funcs: VizierQueryFunc[]): ExecuteScriptRequest {
    const req = new ExecuteScriptRequest();
    const errors = [];
    req.setClusterId(this.clusterID);
    req.setQueryStr(script);
    funcs.forEach((input: VizierQueryFunc) => {
      const execFuncPb = new ExecuteScriptRequest.FuncToExecute();
      execFuncPb.setFuncName(input.name);
      execFuncPb.setOutputTablePrefix(input.outputTablePrefix);
      for (const arg of input.args) {
        const argValPb = new ExecuteScriptRequest.FuncToExecute.ArgValue();
        argValPb.setName(arg.name);
        if (!arg.value) {
          errors.push(`No value provided for arg ${arg.name}.`);
          continue;
        }
        argValPb.setValue(arg.value);
        execFuncPb.addArgValues(argValPb);
      }
      req.addExecFuncs(execFuncPb);
    });

    if (errors.length > 0) {
      throw errors;
    }
    return req;
  }
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
