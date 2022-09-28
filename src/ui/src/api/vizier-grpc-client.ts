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

import { ClientReadableStream } from 'grpc-web';
import { compactDecrypt } from 'jose';
import * as pako from 'pako';
import { Observable, of, from } from 'rxjs';
import {
  bufferTime, catchError, concatMap, finalize, mergeMap, map, timeout, startWith,
} from 'rxjs/operators';

import {
  ErrorDetails, ExecuteScriptRequest, HealthCheckRequest, QueryExecutionStats, Relation,
  RowBatchData, Status, MutationInfo, HealthCheckResponse, ExecuteScriptResponse,
  GenerateOTelScriptRequest, GenerateOTelScriptResponse,
} from 'app/types/generated/vizierapi_pb';
import { VizierServiceClient } from 'app/types/generated/VizierapiServiceClientPb';

import { GRPCStatusCode, VizierQueryError } from './vizier';
import { VizierTable } from './vizier-table';

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

export interface ExecuteScriptOptions {
  enableE2EEncryption: boolean;
}

export interface VizierQueryResult {
  queryId?: string;
  tables: VizierTable[];
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
  table: VizierTable;
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

type KeyPair = {
  publicKeyJWK: JsonWebKey,
  privateKey: CryptoKey,
};

async function parseKeyPair(data: string): Promise<KeyPair> {
  const parsed: { privateKeyJWK: JsonWebKey, publicKeyJWK: JsonWebKey } = JSON.parse(data);
  const privateKey = await window.crypto.subtle.importKey(
    'jwk',
    parsed.privateKeyJWK,
    { name: 'RSA-OAEP', hash: 'SHA-256' },
    true,
    // Only set decrypt as option because this is the private key.
    ['decrypt'],
  );
  return { publicKeyJWK: parsed.publicKeyJWK, privateKey };
}

async function serializeKeyPair(pair: KeyPair): Promise<string> {
  const privateKeyJWK = await window.crypto.subtle.exportKey('jwk', pair.privateKey);
  return JSON.stringify({ privateKeyJWK, publicKeyJWK: pair.publicKeyJWK });
}

async function generateRSAKeyPair(): Promise<KeyPair> {
  const keyPair = await window.crypto.subtle.generateKey(
    {
      name: 'RSA-OAEP',
      modulusLength: 4096,
      publicExponent: new Uint8Array([1, 0, 1]),
      hash: 'SHA-256',
    },
    true,
    ['encrypt', 'decrypt'],
  );
  const publicKeyJWK = await window.crypto.subtle.exportKey(
    'jwk',
    keyPair.publicKey,
  );
  return { publicKeyJWK, privateKey: keyPair.privateKey };
}

let keyPairCache: string;
async function getRSAKeyPair(): Promise<KeyPair> {
  // If key pair exists in our cache, then we parse it out.
  try {
    keyPairCache = sessionStorage.getItem('pixie-e2e-encryption-key');
  } catch (_) {
    // If sessionStorage isn't available, we may be in an incognito tab.
    // Fall back to memory that won't survive a refresh.
  }

  if (keyPairCache) {
    return parseKeyPair(keyPairCache);
  }
  // Generate if not found in keyparse.
  const keyPair = await generateRSAKeyPair();
  keyPairCache = await serializeKeyPair(keyPair);
  try {
    sessionStorage.setItem('pixie-e2e-encryption-key', keyPairCache);
  } catch (_) { /* See above: likely in an incognito tab. */ }
  return keyPair;
}

async function decryptRSA(privateKey: CryptoKey, data: Uint8Array): Promise<Uint8Array> {
  const { plaintext } = await compactDecrypt(data, privateKey, {
    inflateRaw: (input) => Promise.resolve(pako.inflateRaw(input)),
  });
  return plaintext;
}

const HEALTH_CHECK_TIMEOUT = 10000; // 10 seconds

type ExecuteScriptResponseOrError = {
  resp?: ExecuteScriptResponse,
  error?: VizierQueryError,
};

/**
 * Client for gRPC connections to individual Vizier clusters.
 * See CloudGQLClient for the GraphQL client that works with shared data.
 * Should not be used directly - PixieAPIClient exposes wrappers around both this and CloudGQLClient,
 * which smooth out bumps in things like error handling. Prefer using that if you can.
 */
export class VizierGRPCClient {
  private readonly client: VizierServiceClient;

  private readonly rsaKeyPromise: Promise<KeyPair>;

  constructor(
    addr: string,
    private token: string,
    readonly clusterID: string,
  ) {
    this.client = new VizierServiceClient(addr, null, token ? {} : { withCredentials: 'true' });
    // Generate once per client and cache it to avoid the expense of regenrating on every
    // request. The key pair will rotate on browser reload or on creation of a new client.
    this.rsaKeyPromise = getRSAKeyPair();
    withDevTools(this.client);
  }

  /**
    * Monitors the health of both Pixie instrumentation on a cluster, and this client's connection to it.
    * The returned Observable watches
    */
  health(): Observable<Status> {
    const headers = {
      ...(this.token ? { Authorization: `bearer ${this.token}` } : {}),
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

  generateOTelExportScript(script: string): Promise<string | VizierQueryError> {
    const headers = {
      ...(this.token ? { Authorization: `bearer ${this.token}` } : {}),
      // Add a short deadline to the request to account for Viziers that don't
      // have this message implemented. Without this, the user will be stuck
      // waiting for a response and believe the request didn't work.
      deadline: (new Date(Date.now() + 5000)).getTime().toString(),
    };
    const req = new GenerateOTelScriptRequest();
    req.setClusterId(this.clusterID);
    req.setPxlScript(script);

    return this.client.generateOTelScript(req, headers).then(
      (resp: GenerateOTelScriptResponse) => {
        const status = resp.getStatus();
        const errList = status.getErrorDetailsList();
        if (errList.length > 0) {
          return new VizierQueryError('execution', getExecutionErrors(errList), status);
        }
        const errMsg = status.getMessage();
        if (errMsg) {
          return new VizierQueryError('execution', errMsg, status);
        }
        return resp.getOtelScript();
      }).catch((err) => new VizierQueryError('execution', err.message));
  }

  // Use a generator to produce the VizierQueryFunc to remove the dependency on vis.tsx.
  // funcsGenerator should correspond to getQueryFuncs in vis.tsx.
  executeScript(
    script: string,
    funcs: VizierQueryFunc[],
    mutation: boolean,
    opts: ExecuteScriptOptions,
    scriptName: string,
  ): Observable<ExecutionStateUpdate> {
    let call: ClientReadableStream<unknown>;
    const cancelCall = () => {
      if (call?.cancel) {
        call.cancel();
      }
    };
    let keyPair: KeyPair;
    const tablesMap = new Map<string, VizierTable>();
    const results: VizierQueryResult = { tables: [] };

    const rsaKeyPromise: Promise<KeyPair> = opts.enableE2EEncryption ? this.rsaKeyPromise : Promise.resolve(null);

    return from(
      rsaKeyPromise,
    ).pipe(
      mergeMap((kp: KeyPair) => {
        keyPair = kp;
        const headers = {
          ...(this.token ? { Authorization: `bearer ${this.token}` } : {}),
        };
        const req = new ExecuteScriptRequest();
        req.setClusterId(this.clusterID);
        req.setQueryStr(script);
        req.setMutation(mutation);
        req.setQueryName(scriptName);
        if (keyPair) {
          const encOpts = new ExecuteScriptRequest.EncryptionOptions();
          encOpts.setJwkKey(JSON.stringify(keyPair.publicKeyJWK));
          encOpts.setKeyAlg('RSA-OAEP-256');
          encOpts.setContentAlg('A256GCM');
          encOpts.setCompressionAlg('DEF');
          req.setEncryptionOptions(encOpts);
        }
        const err = addFuncsToRequest(req, funcs);
        if (err) {
          throw err;
        }

        call = this.client.executeScript(req, headers);

        return new Observable<ExecuteScriptResponseOrError>((observer) => {
          call.on('data', (data: ExecuteScriptResponse) => {
            observer.next({ resp: data });
          });
          call.on('error', (grpcError) => {
            let error: VizierQueryError;
            if (grpcError.code === GRPCStatusCode.Unavailable) {
              error = new VizierQueryError('unavailable', grpcError.message);
            } else {
              error = new VizierQueryError('server', grpcError.message);
            }
            // Delay throwing of errors until after the buffering step.
            observer.next({ error });
          });
          call.on('end', observer.complete.bind(observer));
        });
      }),
      finalize(cancelCall),
      concatMap((respOrErr: ExecuteScriptResponseOrError) => {
        const { resp, error } = respOrErr;
        if (error) {
          return of(respOrErr);
        }
        if (!keyPair || !resp.hasData()) {
          return of(respOrErr);
        }
        if (!resp.getData().getEncryptedBatch()) {
          if (resp.getData().hasBatch()) {
            // eslint-disable-next-line no-console
            console.warn('Expected table data to be encrypted. Please upgrade vizier.');
          }
          return of(respOrErr);
        }
        const encrypted = resp.getData().getEncryptedBatch_asU8();
        return from(decryptRSA(keyPair.privateKey, encrypted)).pipe(
          map((dec: Uint8Array) => {
            resp.getData().setBatch(RowBatchData.deserializeBinary(dec));
            resp.getData().setEncryptedBatch('');
            return { resp };
          }),
        );
      }),
      bufferTime(250),
      concatMap((buffer: ExecuteScriptResponseOrError[]) => {
        const outs: ExecutionStateUpdate[] = [];
        const dataBatch: BatchDataUpdate[] = [];

        let errored = false;
        for (const respOrErr of buffer) {
          const { resp, error } = respOrErr;
          if (error) {
            errored = true;
            outs.push({ event: { type: 'error', error }, completionReason: 'error', results });
            break;
          }
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
            const id = resp.getMetaData().getId();
            const name = resp.getMetaData().getName();
            const relation = resp.getMetaData().getRelation();
            tablesMap.set(id, new VizierTable(id, name, relation));
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
              table.appendBatch(batch);
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

        if (!errored) {
          outs.push({ event: { type: 'data', data: dataBatch }, results });
        }
        return outs;
      }),
      startWith({
        event: { type: 'start' as const },
        results,
        cancel: cancelCall,
      }),
      catchError((error) => {
        cancelCall();
        return of({
          event: { type: 'error' as const, error }, completionReason: 'error' as const, results,
        });
      }),
      finalize(cancelCall),
    );
  }
}
