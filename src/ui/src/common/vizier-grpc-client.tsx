import {
    ExecuteScriptRequest, QueryExecutionStats, Relation, RowBatchData, Status,
} from 'types/generated/vizier_pb';
import {VizierServiceClient} from 'types/generated/VizierServiceClientPb';

export interface VizierQueryResult {
  tables: Array<{
    relation: Relation;
    data: RowBatchData[];
  }>;
  status?: Status;
  executionStats?: QueryExecutionStats;
}

export class VizierGRPCClient {
  private client: VizierServiceClient;

  constructor(addr: string, private token: string) {
    this.client = new VizierServiceClient(addr);
  }

  executeScript(script: string, args?: {}): Promise<VizierQueryResult> {
    const req = new ExecuteScriptRequest();
    req.setQueryStr(script);

    return new Promise((resolve, reject) => {
      const call = this.client.executeScript(req, { Authorization: `BEARER ${this.token}` });
      const results: VizierQueryResult = { tables: [] };

      let tableIndex = -1;

      call.on('data', (resp) => {
        if (resp.hasStatus()) {
          results.status = resp.getStatus();
          resolve(results);
          return;
        }

        if (resp.hasMetaData()) {
          const relation = resp.getMetaData().getRelation();
          results.tables.push({
            relation,
            data: [],
          });
          tableIndex++;
        } else if (resp.hasData()) {
          const data = resp.getData();
          if (data && data.hasBatch()) {
            results.tables[tableIndex].data.push(data.getBatch());
          } else if (data.hasExecutionStats()) {
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
