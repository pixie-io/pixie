/**
 * @fileoverview gRPC-Web generated client stub for pl.api.vizierpb
 * @enhanceable
 * @public
 */

// GENERATED CODE -- DO NOT EDIT!


import * as grpcWeb from 'grpc-web';


import {
  DebugLogRequest,
  DebugLogResponse,
  ExecuteScriptRequest,
  ExecuteScriptResponse,
  HealthCheckRequest,
  HealthCheckResponse} from './vizier_pb';

export class VizierServiceClient {
  client_: grpcWeb.AbstractClientBase;
  hostname_: string;
  credentials_: null | { [index: string]: string; };
  options_: null | { [index: string]: string; };

  constructor (hostname: string,
               credentials?: null | { [index: string]: string; },
               options?: null | { [index: string]: string; }) {
    if (!options) options = {};
    if (!credentials) credentials = {};
    options['format'] = 'text';

    this.client_ = new grpcWeb.GrpcWebClientBase(options);
    this.hostname_ = hostname;
    this.credentials_ = credentials;
    this.options_ = options;
  }

  methodInfoExecuteScript = new grpcWeb.AbstractClientBase.MethodInfo(
    ExecuteScriptResponse,
    (request: ExecuteScriptRequest) => {
      return request.serializeBinary();
    },
    ExecuteScriptResponse.deserializeBinary
  );

  executeScript(
    request: ExecuteScriptRequest,
    metadata?: grpcWeb.Metadata) {
    return this.client_.serverStreaming(
      this.hostname_ +
        '/pl.api.vizierpb.VizierService/ExecuteScript',
      request,
      metadata || {},
      this.methodInfoExecuteScript);
  }

  methodInfoHealthCheck = new grpcWeb.AbstractClientBase.MethodInfo(
    HealthCheckResponse,
    (request: HealthCheckRequest) => {
      return request.serializeBinary();
    },
    HealthCheckResponse.deserializeBinary
  );

  healthCheck(
    request: HealthCheckRequest,
    metadata?: grpcWeb.Metadata) {
    return this.client_.serverStreaming(
      this.hostname_ +
        '/pl.api.vizierpb.VizierService/HealthCheck',
      request,
      metadata || {},
      this.methodInfoHealthCheck);
  }

}

export class VizierDebugServiceClient {
  client_: grpcWeb.AbstractClientBase;
  hostname_: string;
  credentials_: null | { [index: string]: string; };
  options_: null | { [index: string]: string; };

  constructor (hostname: string,
               credentials?: null | { [index: string]: string; },
               options?: null | { [index: string]: string; }) {
    if (!options) options = {};
    if (!credentials) credentials = {};
    options['format'] = 'text';

    this.client_ = new grpcWeb.GrpcWebClientBase(options);
    this.hostname_ = hostname;
    this.credentials_ = credentials;
    this.options_ = options;
  }

  methodInfoDebugLog = new grpcWeb.AbstractClientBase.MethodInfo(
    DebugLogResponse,
    (request: DebugLogRequest) => {
      return request.serializeBinary();
    },
    DebugLogResponse.deserializeBinary
  );

  debugLog(
    request: DebugLogRequest,
    metadata?: grpcWeb.Metadata) {
    return this.client_.serverStreaming(
      this.hostname_ +
        '/pl.api.vizierpb.VizierDebugService/DebugLog',
      request,
      metadata || {},
      this.methodInfoDebugLog);
  }

}

