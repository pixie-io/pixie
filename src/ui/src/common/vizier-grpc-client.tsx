import {ExecuteQueryResult} from 'gql-types';
import {
    ExecuteScriptRequest, QueryExecutionStats, Relation, RowBatchData, Status,
} from 'types/generated/vizier_pb';
import {VizierServiceClient} from 'types/generated/VizierServiceClientPb';

export interface VizierQueryResult {
  relation?: Relation;
  data?: RowBatchData[];
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
      const results: VizierQueryResult = { data: [] };

      call.on('data', (resp) => {
        if (resp.hasStatus()) {
          resolve({ status: resp.getStatus() });
          return;
        }
        if (resp.hasData()) {
          const data = resp.getData();
          if (data && data.hasBatch()) {
            results.data.push(data.getBatch());
          } else if (data.hasExecutionStats()) {
            results.executionStats = data.getExecutionStats();
            resolve(results);
            return;
          }
        } else if (resp.hasMetaData()) {
          results.relation = resp.getMetaData().getRelation();
        }
      });
      call.on('error', (err) => {
        reject(err);
      });
    });
  }
}
