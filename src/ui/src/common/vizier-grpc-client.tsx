import {Observable} from 'rxjs';
import {
    ExecuteScriptRequest, HealthCheckRequest, QueryExecutionStats, Relation, RowBatchData, Status,
} from 'types/generated/vizier_pb';
import {VizierServiceClient} from 'types/generated/VizierServiceClientPb';

interface Table {
  relation: Relation;
  data: RowBatchData[];
  name: string;
  id: string;
}

export interface VizierQueryResult {
  tables: Table[];
  status?: Status;
  executionStats?: QueryExecutionStats;
}

export class VizierGRPCClient {
  private client: VizierServiceClient;

  constructor(addr: string, private token: string) {
    this.client = new VizierServiceClient(addr);
  }

  health(): Observable<Status> {
    return Observable.create((observer) => {
      const req = new HealthCheckRequest();
      const call = this.client.healthCheck(req,  { Authorization: `BEARER ${this.token}` });
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
    req.setQueryStr(script);

    return new Promise((resolve, reject) => {
      const call = this.client.executeScript(req, { Authorization: `BEARER ${this.token}` });
      const tablesMap = new Map<string, Table>();
      const results: VizierQueryResult = { tables: [] };

      call.on('data', (resp) => {
        if (resp.hasStatus()) {
          results.status = resp.getStatus();
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
