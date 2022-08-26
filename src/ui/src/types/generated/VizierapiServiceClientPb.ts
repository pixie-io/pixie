/**
 * @fileoverview gRPC-Web generated client stub for px.api.vizierpb
 * @enhanceable
 * @public
 */

// GENERATED CODE -- DO NOT EDIT!


/* eslint-disable */
// @ts-nocheck


import * as grpcWeb from 'grpc-web';

import * as src_api_proto_vizierpb_vizierapi_pb from './vizierapi_pb';


export class VizierServiceClient {
  client_: grpcWeb.AbstractClientBase;
  hostname_: string;
  credentials_: null | { [index: string]: string; };
  options_: null | { [index: string]: any; };

  constructor (hostname: string,
               credentials?: null | { [index: string]: string; },
               options?: null | { [index: string]: any; }) {
    if (!options) options = {};
    if (!credentials) credentials = {};
    options['format'] = 'text';

    this.client_ = new grpcWeb.GrpcWebClientBase(options);
    this.hostname_ = hostname;
    this.credentials_ = credentials;
    this.options_ = options;
  }

  methodInfoExecuteScript = new grpcWeb.AbstractClientBase.MethodInfo(
    src_api_proto_vizierpb_vizierapi_pb.ExecuteScriptResponse,
    (request: src_api_proto_vizierpb_vizierapi_pb.ExecuteScriptRequest) => {
      return request.serializeBinary();
    },
    src_api_proto_vizierpb_vizierapi_pb.ExecuteScriptResponse.deserializeBinary
  );

  executeScript(
    request: src_api_proto_vizierpb_vizierapi_pb.ExecuteScriptRequest,
    metadata?: grpcWeb.Metadata) {
    return this.client_.serverStreaming(
      this.hostname_ +
        '/px.api.vizierpb.VizierService/ExecuteScript',
      request,
      metadata || {},
      this.methodInfoExecuteScript);
  }

  methodInfoHealthCheck = new grpcWeb.AbstractClientBase.MethodInfo(
    src_api_proto_vizierpb_vizierapi_pb.HealthCheckResponse,
    (request: src_api_proto_vizierpb_vizierapi_pb.HealthCheckRequest) => {
      return request.serializeBinary();
    },
    src_api_proto_vizierpb_vizierapi_pb.HealthCheckResponse.deserializeBinary
  );

  healthCheck(
    request: src_api_proto_vizierpb_vizierapi_pb.HealthCheckRequest,
    metadata?: grpcWeb.Metadata) {
    return this.client_.serverStreaming(
      this.hostname_ +
        '/px.api.vizierpb.VizierService/HealthCheck',
      request,
      metadata || {},
      this.methodInfoHealthCheck);
  }

  methodInfoGenerateOTelScript = new grpcWeb.AbstractClientBase.MethodInfo(
    src_api_proto_vizierpb_vizierapi_pb.GenerateOTelScriptResponse,
    (request: src_api_proto_vizierpb_vizierapi_pb.GenerateOTelScriptRequest) => {
      return request.serializeBinary();
    },
    src_api_proto_vizierpb_vizierapi_pb.GenerateOTelScriptResponse.deserializeBinary
  );

  generateOTelScript(
    request: src_api_proto_vizierpb_vizierapi_pb.GenerateOTelScriptRequest,
    metadata: grpcWeb.Metadata | null): Promise<src_api_proto_vizierpb_vizierapi_pb.GenerateOTelScriptResponse>;

  generateOTelScript(
    request: src_api_proto_vizierpb_vizierapi_pb.GenerateOTelScriptRequest,
    metadata: grpcWeb.Metadata | null,
    callback: (err: grpcWeb.Error,
               response: src_api_proto_vizierpb_vizierapi_pb.GenerateOTelScriptResponse) => void): grpcWeb.ClientReadableStream<src_api_proto_vizierpb_vizierapi_pb.GenerateOTelScriptResponse>;

  generateOTelScript(
    request: src_api_proto_vizierpb_vizierapi_pb.GenerateOTelScriptRequest,
    metadata: grpcWeb.Metadata | null,
    callback?: (err: grpcWeb.Error,
               response: src_api_proto_vizierpb_vizierapi_pb.GenerateOTelScriptResponse) => void) {
    if (callback !== undefined) {
      return this.client_.rpcCall(
        this.hostname_ +
          '/px.api.vizierpb.VizierService/GenerateOTelScript',
        request,
        metadata || {},
        this.methodInfoGenerateOTelScript,
        callback);
    }
    return this.client_.unaryCall(
    this.hostname_ +
      '/px.api.vizierpb.VizierService/GenerateOTelScript',
    request,
    metadata || {},
    this.methodInfoGenerateOTelScript);
  }

}

export class VizierDebugServiceClient {
  client_: grpcWeb.AbstractClientBase;
  hostname_: string;
  credentials_: null | { [index: string]: string; };
  options_: null | { [index: string]: any; };

  constructor (hostname: string,
               credentials?: null | { [index: string]: string; },
               options?: null | { [index: string]: any; }) {
    if (!options) options = {};
    if (!credentials) credentials = {};
    options['format'] = 'text';

    this.client_ = new grpcWeb.GrpcWebClientBase(options);
    this.hostname_ = hostname;
    this.credentials_ = credentials;
    this.options_ = options;
  }

  methodInfoDebugLog = new grpcWeb.AbstractClientBase.MethodInfo(
    src_api_proto_vizierpb_vizierapi_pb.DebugLogResponse,
    (request: src_api_proto_vizierpb_vizierapi_pb.DebugLogRequest) => {
      return request.serializeBinary();
    },
    src_api_proto_vizierpb_vizierapi_pb.DebugLogResponse.deserializeBinary
  );

  debugLog(
    request: src_api_proto_vizierpb_vizierapi_pb.DebugLogRequest,
    metadata?: grpcWeb.Metadata) {
    return this.client_.serverStreaming(
      this.hostname_ +
        '/px.api.vizierpb.VizierDebugService/DebugLog',
      request,
      metadata || {},
      this.methodInfoDebugLog);
  }

  methodInfoDebugPods = new grpcWeb.AbstractClientBase.MethodInfo(
    src_api_proto_vizierpb_vizierapi_pb.DebugPodsResponse,
    (request: src_api_proto_vizierpb_vizierapi_pb.DebugPodsRequest) => {
      return request.serializeBinary();
    },
    src_api_proto_vizierpb_vizierapi_pb.DebugPodsResponse.deserializeBinary
  );

  debugPods(
    request: src_api_proto_vizierpb_vizierapi_pb.DebugPodsRequest,
    metadata?: grpcWeb.Metadata) {
    return this.client_.serverStreaming(
      this.hostname_ +
        '/px.api.vizierpb.VizierDebugService/DebugPods',
      request,
      metadata || {},
      this.methodInfoDebugPods);
  }

}

