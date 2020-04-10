import {Observable} from 'rxjs';
import {
    ErrorDetails, ExecuteScriptRequest, HealthCheckRequest, QueryExecutionStats, Relation, RowBatchData, Status,
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

  executeScript(script: string, args?: {}): Promise<VizierQueryResult> {
    const req = new ExecuteScriptRequest();
    req.setClusterId(this.clusterID);
    req.setQueryStr(script);

    return new Promise((resolve, reject) => {
      const call = this.client.executeScript(req, this.attachCreds ? {} : { Authorization: `BEARER ${this.token}` });
      const tablesMap = new Map<string, Table>();
      const results: VizierQueryResult = { tables: [] };

      call.on('data', (resp) => {
        if (!results.queryId) {
          results.queryId = resp.getQueryId();
        }

        if (resp.hasStatus()) {
          const status = resp.getStatus();
          if (!status.getErrorDetailsList() || status.getErrorDetailsList().length) {
            const errors = status.getErrorDetailsList().map((error) => {
              switch (error.getErrorCase()) {
                case ErrorDetails.ErrorCase.COMPILER_ERROR: {
                  const ce = error.getCompilerError();
                  return `Compiler error on line ${ce.getLine()}, column ${ce.getColumn()}: ${ce.getMessage()}.`;
                }
                default:
                  return `Unknown error type ${ErrorDetails.ErrorCase[error.getErrorCase()]}.`;
              }
            });
            reject(`Script contains ${errors.length} error${errors.length === 1 ? '' : 's'}: ${errors.join('. ')}`);
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
        reject(err.message);
      });
    });
  }
}
